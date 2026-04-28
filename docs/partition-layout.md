# Partition layout for OTA update
*Jetson Orin Nano* layout example
```text
+------------+--------------+--------------+--------------+--------------+
| bootfiles  |   ROOTFS_A   |   ROOTFS_B   |   SKYTRACK   |  USER_DATA   |
|  vfat 64M  |  ext4 14G    |  ext4 14G    |  ext4 10G    |  free space  |
| boot part  | root_a slot  | root_b slot  | persistent   | persistent   |
+------------+--------------+--------------+--------------+--------------+
```

```text
root@jetson-orin-nano-devkit-nvme:~# lsblk
NAME        MAJ:MIN RM  SIZE RO TYPE MOUNTPOINTS
nvme0n1     259:0    0 59.6G  0 disk 
|-nvme0n1p1 259:1    0   14G  0 part /
|-nvme0n1p2 259:2    0   14G  0 part 
|-nvme0n1p3 259:3    0   64M  0 part /boot/efi
|-nvme0n1p4 259:4    0   10G  0 part /data
`-nvme0n1p5 259:5    0 21.6G  0 part /user_data

root@jetson-orin-nano-devkit-nvme:~# blkid
/dev/nvme0n1p5: LABEL="USER_DATA" UUID="625e0fb2-f467-44da-b6f9-4f202e8840f8" BLOCK_SIZE="4096" TYPE="ext4" PARTLABEL="USER_DATA" PARTUUID="a4f66682-68a1-43ce-a4d6-1b93fb30c654"
/dev/nvme0n1p3: UUID="0E5C-FA00" BLOCK_SIZE="512" TYPE="vfat" PARTLABEL="esp" PARTUUID="a42c45c4-6093-4c73-9bee-ce2f9391e568"
/dev/nvme0n1p1: UUID="88eb092e-0e7d-44d5-889e-7890ff08e8a5" BLOCK_SIZE="4096" TYPE="ext4" PARTLABEL="ROOTFS_A" PARTUUID="04aa8e47-88ca-49cd-a961-76e9ecbf33b8"
/dev/nvme0n1p4: LABEL="SKYTRACK" UUID="51e2928d-e2a7-4e0d-a683-fcf15111bbc9" BLOCK_SIZE="1024" TYPE="ext4" PARTLABEL="SKYTRACK" PARTUUID="9757ce3f-a2a6-4fe5-96ae-5637ed3f8ac7"
/dev/nvme0n1p2: UUID="b55e471b-8656-49c0-b149-505f8cfa6207" BLOCK_SIZE="4096" TYPE="ext4" PARTLABEL="ROOTFS_B" PARTUUID="94153e9d-5925-4f49-b0aa-9a410f463dd0"

```
> Please refer [yocto recipe](https://github.com/uneycom/uav-yocto-build/tree/UAV-1705-create-partition-layout-support-ota/layers/meta-skytrack-jetson/recipes-bsp/tegra-binaries) on how to customize Jetson Orin Nano SSD layout


## Labeling rootfs partitions
root_a: ROOTFS_A
root_b: ROOTFS_B

- This label is useful for `sw-description` creation to identify target slot of rootfs
- SKYTRACK /data partition is used to stored our applications configuration and data
- USER_DATA /user_data is used for customer data

