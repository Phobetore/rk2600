#!/bin/bash

# Vérifier les permissions root
if [[ $EUID -ne 0 ]]; then
    echo "Ce script doit être exécuté en tant que root." >&2
    exit 1
fi

# Vérifier que Docker est installé
if ! command -v docker &> /dev/null; then
    echo "Docker n'est pas installé. Veuillez l'installer avant d'exécuter ce script." >&2
    exit 1
fi

# Variables
DISK_IMG="disk.img"
DISK_SIZE="1G"
MOUNT_DIR="/tmp/my-rootfs"
KERNEL_PATH="../linux-6.12.3/arch/x86/boot/bzImage"  # Modifiez selon le chemin de votre noyau
GRUB_DIR="/usr/lib/grub/i386-pc"
ROOTKIT_PATH="./rootkit.ko"

# Création de l'image disque
echo "Création de l'image disque de taille $DISK_SIZE..."
truncate -s $DISK_SIZE $DISK_IMG

# Partitionnement
echo "Partitionnement de l'image disque..."
parted -s $DISK_IMG mktable msdos
parted -s $DISK_IMG mkpart primary ext4 1 "100%"
parted -s $DISK_IMG set 1 boot on

# Loop device
echo "Configuration du loop device..."
LOOP_DEVICE=$(losetup -Pf --show $DISK_IMG)

# Formatage
echo "Formatage de la partition en ext4..."
mkfs.ext4 "${LOOP_DEVICE}p1"

# Montage
echo "Montage de la partition dans $MOUNT_DIR..."
mkdir -p $MOUNT_DIR
mount "${LOOP_DEVICE}p1" $MOUNT_DIR

# Installation d'Alpine
echo "Installation d'Alpine Linux minimal dans l'image disque..."
docker run --rm -v "$MOUNT_DIR:/my-rootfs" alpine:latest /bin/sh -c '
    apk add --no-cache openrc bash busybox util-linux grub kmod;
    mkdir -p /my-rootfs/{dev,proc,run,sys,boot,home,user,root};
    echo "root:root" | chpasswd;
    echo "alpine-rootkit" > /etc/hostname;
    adduser -D user && echo "user:user" | chpasswd;
    echo "user ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers;
    rc-update add local default;
    rc-update add devfs boot;
    rc-update add procfs boot;
    rc-update add sysfs boot;
    rc-update add mdev sysinit;
'

# Vérification de /bin/sh
echo "Vérification de l'existence de /bin/sh..."
ln -sf /bin/busybox $MOUNT_DIR/bin/sh

# Copie du noyau
echo "Copie du noyau dans l'image disque..."
mkdir -p $MOUNT_DIR/boot
cp "$KERNEL_PATH" $MOUNT_DIR/boot/vmlinuz

# Copie du rootkit
echo "Copie du rootkit dans l'image disque..."
mkdir -p $MOUNT_DIR/home/user
cp "$ROOTKIT_PATH" $MOUNT_DIR/home/user/rootkit.ko
chmod 700 $MOUNT_DIR/home/user/rootkit.ko

# Script de démarrage
echo "Configuration de l'insertion automatique du rootkit..."
mkdir -p $MOUNT_DIR/etc/local.d
cat <<'EOF' > $MOUNT_DIR/etc/local.d/rootkit.start
#!/bin/sh
insmod /home/user/rootkit.ko
EOF
chmod +x $MOUNT_DIR/etc/local.d/rootkit.start

# Configuration de GRUB
echo "Installation de GRUB..."
mkdir -p $MOUNT_DIR/boot/grub
cat <<EOF > $MOUNT_DIR/boot/grub/grub.cfg
serial
terminal_input serial
terminal_output serial
set root=(hd0,1)
menuentry "Rootkit Test Environment" {
    linux /boot/vmlinuz root=/dev/sda1 rw console=ttyS0 init=/sbin/init
}
EOF
grub-install --directory="$GRUB_DIR" --boot-directory="$MOUNT_DIR/boot" "$LOOP_DEVICE"

# Nettoyage
echo "Nettoyage..."
umount "$MOUNT_DIR"
losetup -d "$LOOP_DEVICE"
rmdir "$MOUNT_DIR"

# Test avec QEMU
echo "Démarrage de l'image disque avec QEMU..."
qemu-system-x86_64 \
    -hda "$DISK_IMG" \
    -nographic \
    -m 1024 \
    -net nic -net user
