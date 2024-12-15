#!/bin/bash
set -e

########################################
#           Vérifications préalables   #
########################################

# Vérifiez les permissions root
if [[ $EUID -ne 0 ]]; then
    echo "Ce script doit être exécuté en tant que root." >&2
    exit 1
fi

# Vérifiez la présence des outils nécessaires
for cmd in parted losetup mkfs.ext4 docker qemu-system-x86_64 grub-install; do
    if ! command -v $cmd >/dev/null 2>&1; then
        echo "Erreur: L'outil '$cmd' n'est pas installé. Veuillez l'installer." >&2
        exit 1
    fi
done

########################################
#            Variables                 #
########################################

DISK_IMG="2600MinAlpine.img"
DISK_SIZE="2G"
ROOTFS_DIR="/tmp/rootfs"
KERNEL_PATH="../linux-6.12.3/arch/x86/boot/bzImage"
GRUB_DIR="/usr/lib/grub/i386-pc"
ROOTKIT_PATH="./rootkit.ko"

########################################
#              Fonctions               #
########################################

cleanup() {
    set +e
    if mountpoint -q "$ROOTFS_DIR"; then
        umount "$ROOTFS_DIR"
    fi
    if losetup -a | grep -q "$DISK_IMG"; then
        losetup -d "$(losetup -l | grep "$DISK_IMG" | awk '{print $1}')"
    fi
    rm -rf "$ROOTFS_DIR"
}
trap cleanup EXIT

########################################
#      Création de l'image disque      #
########################################

echo "Création de l'image disque de taille $DISK_SIZE..."
truncate -s $DISK_SIZE $DISK_IMG

echo "Partitionnement de l'image disque..."
parted -s $DISK_IMG mktable msdos
parted -s $DISK_IMG mkpart primary ext4 1 "100%"
parted -s $DISK_IMG set 1 boot on

echo "Configuration du loop device..."
LOOP_DEVICE=$(losetup -Pf --show "$DISK_IMG")

echo "Formatage de la partition en ext4..."
mkfs.ext4 "${LOOP_DEVICE}p1"

echo "Montage de la partition..."
mkdir -p "$ROOTFS_DIR"
mount "${LOOP_DEVICE}p1" "$ROOTFS_DIR"

########################################
#     Installation du système Alpine   #
########################################

echo "Installation d'Alpine Linux minimal dans l'image disque..."
docker run --rm -v "$ROOTFS_DIR:/my-rootfs" alpine:latest /bin/sh -c '
    apk add --no-cache openrc bash busybox util-linux grub kmod;
    mkdir -p /my-rootfs/{dev,proc,run,sys,boot,home,user,root};
    echo "root:root" | chpasswd;
    echo "alpine-rootkit" > /etc/hostname;
    adduser -D user && echo "user:user" | chpasswd;
    echo "user ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers;

    # Copie des fichiers nécessaires explicitement
    cp -a /bin /my-rootfs/bin;
    cp -a /sbin /my-rootfs/sbin;
    cp -a /usr /my-rootfs/usr;
    rc-update add local default
'

########################################
# Vérification de `/bin/sh`
########################################

echo "Vérification de l'existence de /bin/sh..."
if [ ! -f "$ROOTFS_DIR/bin/sh" ]; then
    echo "Erreur: /bin/sh est toujours manquant dans l'image disque." >&2
    exit 1
fi



########################################
#       Copie du noyau et rootkit      #
########################################

echo "Copie du noyau dans l'image disque..."
mkdir -p "$ROOTFS_DIR/boot"
cp "$KERNEL_PATH" "$ROOTFS_DIR/boot/vmlinuz"

echo "Copie du rootkit..."
mkdir -p "$ROOTFS_DIR/home/user"
cp "$ROOTKIT_PATH" "$ROOTFS_DIR/home/user/rootkit.ko"
chmod 700 "$ROOTFS_DIR/home/user/rootkit.ko"

########################################
#       Configuration de GRUB et init  #
########################################

echo "Configuration de GRUB et init..."

# Vérifier si /sbin/init existe, sinon créer le lien symbolique
if [ ! -f "$ROOTFS_DIR/sbin/init" ]; then
    echo "Le fichier /sbin/init est manquant. Création du lien symbolique..."
    sudo ln -sf /bin/busybox "$ROOTFS_DIR/sbin/init"
fi

# Vérifier la présence de busybox
if [ ! -f "$ROOTFS_DIR/bin/busybox" ]; then
    echo "Busybox est manquant. Téléchargement et installation..."
    sudo cp /usr/bin/busybox "$ROOTFS_DIR/bin/"
    sudo chmod +x "$ROOTFS_DIR/bin/busybox"
fi

# Créer la configuration GRUB
echo "Création de la configuration GRUB..."
sudo mkdir -p "$ROOTFS_DIR/boot/grub"
cat <<EOF | sudo tee "$ROOTFS_DIR/boot/grub/grub.cfg"
serial
terminal_input serial
terminal_output serial
set root=(hd0,1)
menuentry "Linux2600" {
    linux /boot/vmlinuz root=/dev/sda1 console=ttyS0 rw init=/sbin/init
}
EOF

# Installer GRUB
echo "Installation de GRUB..."
sudo grub-install --directory=/usr/lib/grub/i386-pc --boot-directory="$ROOTFS_DIR/boot" "$LOOP_DEVICE"


########################################
#          Nettoyage                   #
########################################

echo "Démontage et finalisation..."
umount "$ROOTFS_DIR"
losetup -d "$LOOP_DEVICE"
rmdir "$ROOTFS_DIR"

########################################
#         Démarrage de la VM           #
########################################

echo "Démarrage de l'image disque avec QEMU..."
qemu-system-x86_64 \
    -hda "$DISK_IMG" \
    -nographic \
    -m 1024 \
    -net nic -net user
