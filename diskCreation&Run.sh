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
DISK_SIZE="450M"
MOUNT_DIR="/tmp/my-rootfs"
KERNEL_PATH="../linux-6.12.3/arch/x86/boot/bzImage"  # Modifiez selon le chemin de votre noyau
GRUB_DIR="/usr/lib/grub/i386-pc"

echo "Création de l'image disque de taille $DISK_SIZE..."
truncate -s $DISK_SIZE $DISK_IMG

# Étape 1 : Partitionnement de l'image disque
echo "Partitionnement de l'image disque..."
parted -s $DISK_IMG mktable msdos
parted -s $DISK_IMG mkpart primary ext4 1 "100%"
parted -s $DISK_IMG set 1 boot on

# Étape 2 : Association avec un loop device
echo "Configuration du loop device..."
LOOP_DEVICE=$(losetup -Pf --show $DISK_IMG)

# Étape 3 : Formater la partition en ext4
echo "Formatage de la partition en ext4..."
mkfs.ext4 "${LOOP_DEVICE}p1"

# Étape 4 : Monter la partition
echo "Montage de la partition dans $MOUNT_DIR..."
mkdir -p $MOUNT_DIR
mount "${LOOP_DEVICE}p1" $MOUNT_DIR

# Étape 5 : Installer Alpine Linux minimal
echo "Installation d'Alpine Linux minimal dans l'image disque..."
docker run -it --rm -v $MOUNT_DIR:/my-rootfs alpine sh -c '
  apk add openrc util-linux build-base;
  ln -s agetty /etc/init.d/agetty.ttyS0;
  echo ttyS0 > /etc/securetty;
  rc-update add agetty.ttyS0 default;
  rc-update add devfs boot;
  rc-update add procfs boot;
  rc-update add sysfs boot;
  rc-update add root default;
  ln -sf /bin/busybox $MOUNT_DIR/bin/sh

  # Configurer le compte root
  echo "root:rootpassword" | chpasswd;

  # Ajouter un compte utilisateur standard
  adduser -D user;
  echo "user:userpassword" | chpasswd;

  # Copier les répertoires nécessaires
  for d in bin etc lib root sbin usr; do tar c "/$d" | tar x -C /my-rootfs; done;
  for dir in dev proc run sys var; do mkdir /my-rootfs/${dir}; done;
'

# Vérification et ajout du fichier init
echo "Vérification de l'existence du fichier init..."
if [ ! -f "$MOUNT_DIR/sbin/init" ]; then
    echo "Le fichier init est manquant, création d'un lien symbolique vers /bin/busybox..."
    ln -sf /bin/busybox $MOUNT_DIR/sbin/init
fi

# Étape 6 : Copier le noyau
echo "Copie du noyau dans l'image..."
mkdir -p $MOUNT_DIR/boot
cp $KERNEL_PATH $MOUNT_DIR/boot/vmlinuz

# Étape 6b : Copier le rootkit
ROOTKIT_PATH="./rootkit.ko"
if [ ! -f "$ROOTKIT_PATH" ]; then
    echo "Erreur : Le fichier rootkit.ko est introuvable." >&2
    exit 1
fi

echo "Copie du rootkit dans l'image disque..."
mkdir -p $MOUNT_DIR/home/user
cp $ROOTKIT_PATH $MOUNT_DIR/home/user/rootkit.ko
chmod 700 $MOUNT_DIR/home/user/rootkit.ko

# Ajout d'un script pour insérer le rootkit au démarrage
echo "Configuration de l'insertion automatique du rootkit..."
mkdir -p $MOUNT_DIR/etc/local.d
cat <<'EOF' > $MOUNT_DIR/etc/local.d/rootkit.start
#!/bin/sh
insmod /home/user/rootkit.ko
EOF
chmod +x $MOUNT_DIR/etc/local.d/rootkit.start


# Étape 7 : Configurer GRUB
echo "Installation de GRUB..."
mkdir -p $MOUNT_DIR/boot/grub
cat <<EOF > $MOUNT_DIR/boot/grub/grub.cfg
serial
terminal_input serial
terminal_output serial
set root=(hd0,1)
menuentry "Linux2600" {
    linux /boot/vmlinuz root=/dev/sda1 console=ttyS0 module.sig_enforce=0 init=/sbin/init
}
EOF
grub-install --directory=$GRUB_DIR --boot-directory=$MOUNT_DIR/boot $LOOP_DEVICE

# Étape 8 : Nettoyage
echo "Nettoyage..."
umount $MOUNT_DIR
losetup -d $LOOP_DEVICE
rmdir $MOUNT_DIR

echo "Création de l'image disque terminée : $DISK_IMG"