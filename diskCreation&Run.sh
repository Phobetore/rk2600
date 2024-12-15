#!/bin/bash
set -e

########################################
#           Vérifications host          #
########################################

# Vérifier que les outils nécessaires sont présents sur la machine hôte
for cmd in parted losetup mkfs.ext4 docker qemu-system-x86_64 grub-install; do
    if ! command -v $cmd >/dev/null 2>&1; then
        echo "Erreur: L'outil '$cmd' n'est pas disponible sur votre système hôte."
        exit 1
    fi
done

########################################
#      Gestion des paramètres           #
########################################

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <path_to_bzImage> <path_to_rootkit_directory>"
    exit 1
fi

BZIMAGE_PATH="$1"
ROOTKIT_DIR="$2"

if [ ! -f "$BZIMAGE_PATH" ]; then
    echo "Erreur: Le fichier bzImage '$BZIMAGE_PATH' n'existe pas."
    exit 1
fi

if [ ! -d "$ROOTKIT_DIR" ]; then
    echo "Erreur: Le répertoire '$ROOTKIT_DIR' n'existe pas."
    exit 1
fi

########################################
#      Variables et préparation         #
########################################

DISK_IMG="2600MinAlpine.img"
DISK_SIZE="5G"
ROOTFS_DIR="/tmp/my-rootfs"
ALPINE_VERSION="latest"  # Version d'Alpine Linux à utiliser
LOOP_DEVICE=""

cleanup() {
    set +e
    if mount | grep "$ROOTFS_DIR" >/dev/null 2>&1; then
        sudo umount "$ROOTFS_DIR"
    fi
    if losetup -a | grep "$DISK_IMG" >/dev/null 2>&1; then
        LOOP_DEVICE=$(losetup -l | grep $DISK_IMG | awk '{print $1}')
        sudo losetup -d "$LOOP_DEVICE"
    fi
    rm -rf "$ROOTFS_DIR"
}
trap 'cleanup' EXIT

########################################
#      Création de l'image disque       #
########################################

echo "Nettoyage des anciens loop devices..."
losetup -a | grep $DISK_IMG | cut -d ':' -f1 | while read loop; do
    sudo losetup -d $loop
done

echo "Création de l'image disque..."
truncate -s $DISK_SIZE $DISK_IMG

echo "Création de la table de partition..."
/sbin/parted -s $DISK_IMG mktable msdos
/sbin/parted -s $DISK_IMG mkpart primary ext4 1 "100%"
/sbin/parted -s $DISK_IMG set 1 boot on

echo "Configuration du loop device..."
sudo losetup -Pf $DISK_IMG
LOOP_DEVICE=$(losetup -l | grep $DISK_IMG | awk '{print $1}')

echo "Formatage de la partition en ext4..."
sudo mkfs.ext4 -F ${LOOP_DEVICE}p1

echo "Montage de la partition..."
mkdir -p $ROOTFS_DIR
sudo mount -o rw ${LOOP_DEVICE}p1 $ROOTFS_DIR

########################################
#     Installation du système Alpine    #
########################################

echo "Installation d'Alpine Linux minimal dans le chroot..."
# Installation des paquets nécessaires directement dans le rootfs via Docker
docker run --rm -v $ROOTFS_DIR:/my-rootfs alpine:$ALPINE_VERSION /bin/ash -c "
    apk add --no-cache openrc bash busybox util-linux sudo gcc make kmod grub-bios build-base;
    echo 'root:root' | chpasswd;
    echo 'alpine-rootkit' > /etc/hostname;
    adduser -D user && echo 'user:user' | chpasswd;
    echo 'user    ALL=(ALL:ALL) ALL' >> /etc/sudoers;
"

# Préparation du chroot
sudo chroot $ROOTFS_DIR /bin/ash -c "
    mkdir -p /proc /sys /dev /run &&
    mount -t proc none /proc &&
    mount -t sysfs none /sys &&
    echo 'Configuration utilisateur terminée.'
"

########################################
#           Configuration Noyau         #
########################################

echo "Copie du noyau bzImage..."
sudo mkdir -p $ROOTFS_DIR/boot
sudo cp $BZIMAGE_PATH $ROOTFS_DIR/boot/bzImage

########################################
#         Transfert du rootkit          #
########################################

echo "Transfert du rootkit dans le système invité..."
sudo cp rootkit.ko $ROOTFS_DIR/home/user/rootkit/
sudo chown user:user $ROOTFS_DIR/home/user/rootkit/rootkit.ko
sudo chmod 700 $ROOTFS_DIR/home/user/rootkit/rootkit.ko


########################################
#      Script d'exécution rootkit       #
########################################
# Utilisation du mécanisme /etc/local.d pour lancer le rootkit au démarrage
echo "Ajout du script d'exécution automatique du rootkit..."
sudo mkdir -p $ROOTFS_DIR/etc/local.d
cat <<'EOF' | sudo tee $ROOTFS_DIR/etc/local.d/run_rootkit.start
#!/bin/ash
echo "Insertion du rootkit compilé..."
cd /home/user/rootkit
insmod ./rootkit.ko
EOF

sudo chmod +x $ROOTFS_DIR/etc/local.d/run_rootkit.start

# Activer le service 'local' au démarrage
sudo chroot $ROOTFS_DIR /bin/ash -c "
    rc-update add local default
"

########################################
#       Configuration de GRUB           #
########################################

echo "Configuration de GRUB..."
sudo mkdir -p $ROOTFS_DIR/boot/grub
cat <<EOF | sudo tee $ROOTFS_DIR/boot/grub/grub.cfg
set timeout=5
set default=0
menuentry "2600 minimal Alpine" {
    linux /boot/bzImage root=/dev/sda1 console=ttyS0 rw
}
EOF

sudo grub-install --directory=/usr/lib/grub/i386-pc --boot-directory=$ROOTFS_DIR/boot $LOOP_DEVICE

########################################
#                Nettoyage              #
########################################

echo "Démontage de l'image..."
sudo umount $ROOTFS_DIR
sudo losetup -d $LOOP_DEVICE
rm -rf $ROOTFS_DIR

########################################
#        Démarrage de la VM QEMU        #
########################################

echo "Démarrage de l'image avec QEMU..."
qemu-system-x86_64 \
    -hda $DISK_IMG \
    -nographic \
    -enable-kvm \
    -m 1024 \
    -net nic -net user
