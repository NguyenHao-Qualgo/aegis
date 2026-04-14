#include "aegis/bundle.h"
#include "aegis/checksum.h"
#include "aegis/context.h"
#include "aegis/crypt.h"
#include "aegis/mount.h"
#include "aegis/utils.h"
#include "aegis/verity_hash.h"

#include <cstring>
#include <fstream>
#include <sstream>
#include <sys/mount.h>

namespace aegis {

static Result<void> create_squashfs(const std::string& content_dir,
                                     const std::string& output_path,
                                     const std::string& extra_args) {
    std::vector<std::string> cmd = {"mksquashfs", content_dir, output_path,
                                     "-all-root", "-noappend"};
    if (!extra_args.empty()) {
        // Split extra_args and append
        std::istringstream iss(extra_args);
        std::string arg;
        while (iss >> arg) cmd.push_back(arg);
    }

    auto res = run_command(cmd);
    if (res.exit_code != 0)
        return Result<void>::err("mksquashfs failed: " + res.stderr_str);
    return Result<void>::ok();
}

Result<void> bundle_create(const BundleCreateParams& params) {
    LOG_INFO("Creating '%s' format bundle", to_string(params.format));

    // 1. Parse the manifest from the content directory
    std::string manifest_path = params.content_dir + "/manifest.aegism";
    if (!path_exists(manifest_path))
        return Result<void>::err("No manifest.aegism in content directory");

    auto manifest = parse_manifest(manifest_path);
    manifest.bundle_format = params.format;

    // 2. Compute checksums for all images and update manifest
    for (auto& img : manifest.images) {
        std::string img_path = params.content_dir + "/" + img.filename;
        if (!path_exists(img_path))
            return Result<void>::err("Image file not found: " + img_path);

        auto cs = compute_checksum(img_path);
        img.sha256 = cs.digest;
        img.size   = cs.size;
        LOG_INFO("Image %s: sha256=%s size=%lu", img.filename.c_str(),
                 cs.digest.c_str(), cs.size);
    }

    // 3. Write updated manifest back (without verity/crypt metadata yet)
    write_manifest(manifest, manifest_path);

    // 4. Create squashfs from content directory
    std::string squashfs_path = params.output_path + ".sqsh.tmp";
    auto sqres = create_squashfs(params.content_dir, squashfs_path,
                                  params.mksquashfs_args);
    if (!sqres) return sqres;

    uint64_t sqfs_size = file_size(squashfs_path);
    LOG_INFO("SquashFS payload: %lu bytes", sqfs_size);

    if (params.format == BundleFormat::Plain) {
        // Plain format: squashfs + appended CMS signature
        // Sign the entire squashfs
        SigningParams sign_params;
        sign_params.cert_path = params.cert_path;
        sign_params.key_path  = params.key_path;

        auto sig = cms_sign_file(squashfs_path, sqfs_size, sign_params);

        // Write: squashfs | signature | signature_size(uint64_t)
        copy_file(squashfs_path, params.output_path);
        FILE* f = fopen(params.output_path.c_str(), "ab");
        if (!f) return Result<void>::err("Cannot open output bundle");
        fwrite(sig.data(), 1, sig.size(), f);
        uint64_t sig_size = sig.size();
        fwrite(&sig_size, sizeof(sig_size), 1, f);
        fclose(f);

    } else if (params.format == BundleFormat::Verity) {
        // Verity format: squashfs + hash_tree, CMS contains manifest with verity metadata
        copy_file(squashfs_path, params.output_path);

        // Compute verity hash tree and append to bundle
        auto verity = compute_verity_hash(params.output_path, sqfs_size);
        manifest.verity_hash = verity.root_hash;
        manifest.verity_salt = verity.salt;
        manifest.bundle_verity_size = verity.hash_size;

        LOG_INFO("Verity root_hash=%s salt=%s hash_size=%lu",
                 verity.root_hash.c_str(), verity.salt.c_str(), verity.hash_size);

        // Write the manifest with verity metadata to a temp file, then sign it
        std::string manifest_tmp = params.output_path + ".manifest.tmp";
        write_manifest(manifest, manifest_tmp);
        auto manifest_content = read_file_bytes(manifest_tmp);

        SigningParams sign_params;
        sign_params.cert_path = params.cert_path;
        sign_params.key_path  = params.key_path;

        // For verity, the manifest is embedded (not detached) in the CMS
        auto sig = cms_sign(manifest_content, sign_params);

        // Append CMS signature to bundle
        FILE* f = fopen(params.output_path.c_str(), "ab");
        fwrite(sig.data(), 1, sig.size(), f);
        uint64_t sig_size = sig.size();
        fwrite(&sig_size, sizeof(sig_size), 1, f);
        fclose(f);

        rm_rf(manifest_tmp);

    } else if (params.format == BundleFormat::Crypt) {
        // Crypt format: encrypt squashfs, then verity on top, CMS encrypted to recipients
        std::string enc_path = params.output_path + ".enc.tmp";
        auto aes_key = crypt_encrypt(squashfs_path, enc_path, sqfs_size);

        // Copy encrypted payload as the bundle base
        copy_file(enc_path, params.output_path);
        uint64_t enc_size = file_size(params.output_path);

        // Compute verity over the encrypted payload
        auto verity = compute_verity_hash(params.output_path, enc_size);

        manifest.crypt_key   = aes_key;
        manifest.verity_hash = verity.root_hash;
        manifest.verity_salt = verity.salt;
        manifest.bundle_verity_size = verity.hash_size;

        // Write manifest, sign it, then encrypt the CMS to recipients
        std::string manifest_tmp = params.output_path + ".manifest.tmp";
        write_manifest(manifest, manifest_tmp);
        auto manifest_content = read_file_bytes(manifest_tmp);

        SigningParams sign_params;
        sign_params.cert_path = params.cert_path;
        sign_params.key_path  = params.key_path;

        auto signed_cms = cms_sign(manifest_content, sign_params);

        // Encrypt the signed CMS to all recipients
        Bytes encrypted_cms;
        if (!params.encryption_recipients.empty()) {
            encrypted_cms = cms_encrypt(signed_cms, params.encryption_recipients);
        } else {
            encrypted_cms = signed_cms; // No encryption recipients = not encrypted
        }

        // Append encrypted CMS to bundle
        FILE* f = fopen(params.output_path.c_str(), "ab");
        fwrite(encrypted_cms.data(), 1, encrypted_cms.size(), f);
        uint64_t sig_size = encrypted_cms.size();
        fwrite(&sig_size, sizeof(sig_size), 1, f);
        fclose(f);

        rm_rf(enc_path);
        rm_rf(manifest_tmp);
    }

    // Cleanup temp squashfs
    rm_rf(squashfs_path);

    LOG_INFO("Bundle created: %s (%lu bytes)", params.output_path.c_str(),
             file_size(params.output_path));
    return Result<void>::ok();
}

/// Read the CMS signature from the end of a bundle file.
/// Layout: payload | cms_data | cms_size(uint64_t)
static Result<Bytes> extract_bundle_signature(const std::string& path, uint64_t& payload_size) {
    uint64_t total = file_size(path);
    if (total < sizeof(uint64_t))
        return Result<Bytes>::err("Bundle too small");

    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return Result<Bytes>::err("Cannot open bundle");

    // Read signature size from last 8 bytes
    uint64_t sig_size;
    fseek(f, -static_cast<long>(sizeof(sig_size)), SEEK_END);
    if (fread(&sig_size, sizeof(sig_size), 1, f) != 1) {
        fclose(f);
        return Result<Bytes>::err("Cannot read signature size");
    }

    if (sig_size == 0 || sig_size > total) {
        fclose(f);
        return Result<Bytes>::err("Invalid signature size: " + std::to_string(sig_size));
    }

    payload_size = total - sig_size - sizeof(sig_size);

    // Read signature
    Bytes sig(sig_size);
    fseek(f, static_cast<long>(payload_size), SEEK_SET);
    if (fread(sig.data(), 1, sig_size, f) != sig_size) {
        fclose(f);
        return Result<Bytes>::err("Cannot read signature data");
    }
    fclose(f);

    return Result<Bytes>::ok(std::move(sig));
}

Result<Bundle> bundle_open(const std::string& path,
                           const SigningParams& verify_params) {
    Bundle bundle;
    bundle.path = path;
    bundle.size = file_size(path);

    // Extract signature from end of bundle
    uint64_t payload_size = 0;
    auto sig_result = extract_bundle_signature(path, payload_size);
    if (!sig_result)
        return Result<Bundle>::err("Cannot extract signature: " + sig_result.error());

    auto& sig = sig_result.value();

    // Try to verify and extract manifest from CMS
    // For verity/crypt: manifest is embedded in CMS (not detached)
    try {
        Bytes manifest_bytes;

        // First try: embedded manifest (verity/crypt format)
        try {
            manifest_bytes = cms_verify(sig, verify_params);
        } catch (const SignatureError& e) {
            // If embedded verification fails, this might be a crypt bundle
            // where the CMS is encrypted. Try decrypting first.
            auto& ctx = Context::instance();
            if (!ctx.config().encryption_key.empty()) {
                auto decrypted = cms_decrypt(sig,
                    ctx.config().encryption_key,
                    ctx.config().encryption_cert);
                manifest_bytes = cms_verify(decrypted, verify_params);
            } else {
                throw;
            }
        }

        // Parse manifest from verified content
        std::string manifest_str(manifest_bytes.begin(), manifest_bytes.end());
        // Write to temp file for parsing
        std::string tmp_manifest = "/tmp/aegis-manifest-" + random_hex(4) + ".aegism";
        write_text_file(tmp_manifest, manifest_str);
        bundle.manifest = parse_manifest(tmp_manifest);
        rm_rf(tmp_manifest);

        bundle.format = bundle.manifest.bundle_format;
        bundle.verified = true;

    } catch (const SignatureError& e) {
        return Result<Bundle>::err(std::string("Signature verification failed: ") + e.what());
    }

    LOG_INFO("Bundle opened: format=%s compatible=%s version=%s",
             to_string(bundle.format),
             bundle.manifest.compatible.c_str(),
             bundle.manifest.version.c_str());

    return Result<Bundle>::ok(std::move(bundle));
}

Result<void> bundle_mount(Bundle& bundle) {
    if (bundle.mounted) return Result<void>::ok();

    auto& ctx = Context::instance();
    std::string mount_prefix = ctx.mount_prefix();

    uint64_t payload_size = 0;
    auto sig_result = extract_bundle_signature(bundle.path, payload_size);
    if (!sig_result) {
        return Result<void>::err("Cannot extract signature: " + sig_result.error());
    }

    try {
        if (bundle.format == BundleFormat::Plain) {
            auto mp = mount_squashfs(bundle.path, mount_prefix);
            if (!mp) return Result<void>::err(mp.error());
            bundle.mount_point = mp.value();

        } else if (bundle.format == BundleFormat::Verity) {
            const uint64_t data_size =
                payload_size - bundle.manifest.bundle_verity_size;

            bundle.loop = loop_setup(bundle.path, 0, payload_size);

            bundle.dm_verity = dm_verity_setup(
                bundle.loop.path,
                data_size,
                bundle.manifest.verity_hash,
                bundle.manifest.verity_salt,
                data_size);

            std::string mp = create_mount_point(mount_prefix, "bundle");
            auto res = mount(bundle.dm_verity.dm_device, mp, "squashfs", MS_RDONLY);
            if (!res) return Result<void>::err("Mount verity bundle failed: " + res.error());
            bundle.mount_point = mp;

        } else if (bundle.format == BundleFormat::Crypt) {
            const uint64_t enc_size =
                payload_size - bundle.manifest.bundle_verity_size;

            bundle.loop = loop_setup(bundle.path, 0, payload_size);

            bundle.dm_crypt = dm_crypt_setup(
                bundle.loop.path,
                enc_size,
                bundle.manifest.crypt_key);

            bundle.dm_verity = dm_verity_setup(
                bundle.dm_crypt.dm_device,
                enc_size,
                bundle.manifest.verity_hash,
                bundle.manifest.verity_salt,
                enc_size);

            std::string mp = create_mount_point(mount_prefix, "bundle");
            auto res = mount(bundle.dm_verity.dm_device, mp, "squashfs", MS_RDONLY);
            if (!res) return Result<void>::err("Mount crypt bundle failed: " + res.error());
            bundle.mount_point = mp;
        }

        bundle.mounted = true;
        LOG_INFO("Bundle mounted at %s", bundle.mount_point.c_str());
        return Result<void>::ok();

    } catch (const std::exception& e) {
        return Result<void>::err(std::string("Bundle mount failed: ") + e.what());
    }
}

Result<void> bundle_unmount(Bundle& bundle) {
    if (!bundle.mounted) {
        return Result<void>::ok();
    }

    umount(bundle.mount_point);

    if (bundle.dm_verity.active) {
        dm_remove(bundle.dm_verity.dm_name);
        bundle.dm_verity = {};
    }

    if (bundle.dm_crypt.active) {
        dm_remove(bundle.dm_crypt.dm_name);
        bundle.dm_crypt = {};
    }

    bundle.mounted = false;
    bundle.mount_point.clear();

    LOG_INFO("Bundle unmounted");
    return Result<void>::ok();
}

Result<void> bundle_extract(const Bundle& bundle, const std::string& dest_dir) {
    if (!bundle.mounted)
        return Result<void>::err("Bundle not mounted");

    mkdir_p(dest_dir);
    auto res = run_command({"cp", "-a", bundle.mount_point + "/.", dest_dir + "/"});
    if (res.exit_code != 0)
        return Result<void>::err("Extract failed: " + res.stderr_str);

    LOG_INFO("Bundle extracted to %s", dest_dir.c_str());
    return Result<void>::ok();
}

Result<void> bundle_resign(const std::string& input_path,
                           const std::string& output_path,
                           const SigningParams& old_params,
                           const SigningParams& new_params) {
    // Open with old keys
    auto bundle_res = bundle_open(input_path, old_params);
    if (!bundle_res) return Result<void>::err(bundle_res.error());

    auto& bundle = bundle_res.value();
    LOG_INFO("Resigning '%s' format bundle", to_string(bundle.format));

    // Extract payload (everything before signature)
    uint64_t payload_size = 0;
    extract_bundle_signature(input_path, payload_size);

    // Copy payload to output
    FILE* fin = fopen(input_path.c_str(), "rb");
    FILE* fout = fopen(output_path.c_str(), "wb");
    if (!fin || !fout) return Result<void>::err("Cannot open files for resign");

    std::vector<uint8_t> buf(65536);
    uint64_t remaining = payload_size;
    while (remaining > 0) {
        size_t rd = fread(buf.data(), 1, std::min(static_cast<uint64_t>(buf.size()), remaining), fin);
        fwrite(buf.data(), 1, rd, fout);
        remaining -= rd;
    }
    fclose(fin);

    // Re-sign the manifest with new keys
    std::string tmp_manifest = "/tmp/aegis-resign-" + random_hex(4) + ".aegism";
    write_manifest(bundle.manifest, tmp_manifest);
    auto manifest_content = read_file_bytes(tmp_manifest);

    auto sig = cms_sign(manifest_content, new_params);
    fwrite(sig.data(), 1, sig.size(), fout);
    uint64_t sig_size = sig.size();
    fwrite(&sig_size, sizeof(sig_size), 1, fout);
    fclose(fout);

    rm_rf(tmp_manifest);

    LOG_INFO("Bundle resigned: %s", output_path.c_str());
    return Result<void>::ok();
}

Result<Bundle> bundle_info(const std::string& path,
                           const SigningParams& params,
                           bool no_verify) {
    if (no_verify) {
        // Just read the signature without verifying
        Bundle bundle;
        bundle.path = path;
        bundle.size = file_size(path);
        // Still extract and parse manifest for display
        uint64_t payload_size = 0;
        auto sig_result = extract_bundle_signature(path, payload_size);
        if (!sig_result)
            return Result<Bundle>::err(sig_result.error());
        // Can't parse manifest without verification in production
        // but for info display, try anyway
        bundle.verified = false;
        return Result<Bundle>::ok(std::move(bundle));
    }
    return bundle_open(path, params);
}

} // namespace aegis
