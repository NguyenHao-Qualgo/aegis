#include "DeviceParser.h"

#include <tinyxml2.h>

#include <sstream>

using namespace tinyxml2;

DeviceInformation DeviceParser::parseDeviceInformation(const std::string& xml) {
    DeviceInformation info;
    tinyxml2::XMLDocument doc;
    if (doc.Parse(xml.c_str()) != tinyxml2::XML_SUCCESS)
        return info;

    auto* root = doc.RootElement();
    if (!root || std::string(root->Name()) != "response")
        return info;

    auto* el = root->FirstChildElement("CameraModel");
    if (el && el->GetText())
        info.model = el->GetText();
    if (info.model == "L6D") {
        info.manufacturer = "Motorola";
    }

    el = root->FirstChildElement("Version");
    if (el && el->GetText())
        info.firmwareVersion = el->GetText();

    el = root->FirstChildElement("SerialNumber");
    if (el && el->GetText())
        info.serialNumber = el->GetText();

    el = root->FirstChildElement("PartNumber");
    if (el && el->GetText())
        info.hardwareId = el->GetText();

    return info;
}

VideoSourceConfiguration DeviceParser::parseVideoSourceConfig(const std::string& xml) {
    VideoSourceConfiguration config;
    tinyxml2::XMLDocument doc;
    if (doc.Parse(xml.c_str()) != tinyxml2::XML_SUCCESS)
        return config;

    auto* root = doc.RootElement();
    if (!root)
        return config;

    auto* el = root->FirstChildElement("ConfigToken");
    if (el && el->GetText())
        config.configToken = el->GetText();

    el = root->FirstChildElement("SourceToken");
    if (el && el->GetText())
        config.sourceToken = el->GetText();

    el = root->FirstChildElement("ConfigName");
    if (el && el->GetText())
        config.configName = el->GetText();

    el = root->FirstChildElement("UseCount");
    if (el && el->GetText())
        config.useCount = std::stoi(el->GetText());

    el = root->FirstChildElement("FixedConfig");
    if (el && el->GetText())
        config.fixedConfig = std::string(el->GetText()) == "true";

    auto* rect = root->FirstChildElement("RectangleBoundConfig");
    if (rect) {
        auto* x = rect->FirstChildElement("xBound");
        if (x && x->GetText())
            config.xBound = std::stoi(x->GetText());
        auto* y = rect->FirstChildElement("yBound");
        if (y && y->GetText())
            config.yBound = std::stoi(y->GetText());
        auto* w = rect->FirstChildElement("Width");
        if (w && w->GetText())
            config.width = std::stoi(w->GetText());
        auto* h = rect->FirstChildElement("Height");
        if (h && h->GetText())
            config.height = std::stoi(h->GetText());
    }

    el = root->FirstChildElement("StreamUri");
    if (el && el->GetText())
        config.streamUri = el->GetText();

    el = root->FirstChildElement("StreamChannel");
    if (el && el->GetText())
        config.streamChannel = el->GetText();

    auto* ports = root->FirstChildElement("StreamPorts");
    if (ports) {
        auto* rtsp = ports->FirstChildElement("RTSP-Port");
        if (rtsp && rtsp->GetText())
            config.rtspPort = std::stoi(rtsp->GetText());
        auto* rtsps = ports->FirstChildElement("RTSPS-Port");
        if (rtsps && rtsps->GetText())
            config.rtspsPort = std::stoi(rtsps->GetText());
        auto* http = ports->FirstChildElement("HTTP-Port");
        if (http && http->GetText())
            config.httpPort = std::stoi(http->GetText());
    }

    el = root->FirstChildElement("VersionNumber");
    if (el && el->GetText())
        config.versionNumber = std::stoi(el->GetText());

    return config;
}

VideoEncoderConfiguration DeviceParser::parseVideoEncoderConfig(const std::string& xml) {
    VideoEncoderConfiguration config;
    tinyxml2::XMLDocument doc;
    if (doc.Parse(xml.c_str()) != tinyxml2::XML_SUCCESS)
        return config;

    auto* root = doc.RootElement();
    if (!root)
        return config;

    auto* el = root->FirstChildElement("ConfigToken");
    if (el && el->GetText())
        config.configToken = el->GetText();

    el = root->FirstChildElement("ConfigName");
    if (el && el->GetText())
        config.configName = el->GetText();

    el = root->FirstChildElement("FixedConfig");
    if (el && el->GetText())
        config.fixedConfig = std::string(el->GetText()) == "true";

    el = root->FirstChildElement("UseCount");
    if (el && el->GetText())
        config.useCount = std::stoi(el->GetText());

    auto* res = root->FirstChildElement("VideoResolution");
    if (res) {
        auto* w = res->FirstChildElement("Width");
        if (w && w->GetText())
            config.width = std::stoi(w->GetText());
        auto* h = res->FirstChildElement("Height");
        if (h && h->GetText())
            config.height = std::stoi(h->GetText());
    }

    el = root->FirstChildElement("Encoder");
    if (el && el->GetText())
        config.encoder = el->GetText();

    el = root->FirstChildElement("GuaranteedFramerate");
    if (el && el->GetText())
        config.guaranteedFramerate = std::string(el->GetText()) == "true";

    el = root->FirstChildElement("VideoQuality");
    if (el && el->GetText())
        config.videoQuality = std::stoi(el->GetText());

    el = root->FirstChildElement("SessionTimeout");
    if (el && el->GetText())
        config.sessionTimeout = std::stoi(el->GetText());

    auto* rc = root->FirstChildElement("VideoRateControl");
    if (rc) {
        auto* br = rc->FirstChildElement("BitrateLimit");
        if (br && br->GetText())
            config.bitrateLimit = std::stoi(br->GetText());
        auto* ei = rc->FirstChildElement("EncodingInterval");
        if (ei && ei->GetText())
            config.encodingInterval = std::stoi(ei->GetText());
        auto* fr = rc->FirstChildElement("FrameRateLimit");
        if (fr && fr->GetText())
            config.frameRateLimit = std::stoi(fr->GetText());
    }

    auto* h264 = root->FirstChildElement("H264EncodingConfig");
    if (h264) {
        auto* gov = h264->FirstChildElement("GovLength");
        if (gov && gov->GetText())
            config.govLength = std::stoi(gov->GetText());
        auto* prof = h264->FirstChildElement("H264Profile");
        if (prof && prof->GetText())
            config.h264Profile = prof->GetText();
    }

    el = root->FirstChildElement("MinimumBitrate");
    if (el && el->GetText())
        config.minimumBitrate = std::stoi(el->GetText());

    el = root->FirstChildElement("MinimumFramerate");
    if (el && el->GetText())
        config.minimumFramerate = std::stoi(el->GetText());

    el = root->FirstChildElement("MinimumQuality");
    if (el && el->GetText())
        config.minimumQuality = std::stoi(el->GetText());

    el = root->FirstChildElement("MaximumQuality");
    if (el && el->GetText())
        config.maximumQuality = std::stoi(el->GetText());

    el = root->FirstChildElement("VersionNumber");
    if (el && el->GetText())
        config.versionNumber = std::stoi(el->GetText());

    return config;
}