# README

## Description

Ce projet permet de créer une image disque avec Alpine Linux minimal, un noyau Linux personnalisé, et un rootkit précompilé. Une fois créé, l'image est démarrée dans une machine virtuelle (VM) à l'aide de QEMU, permettant de tester le rootkit en environnement contrôlé.

## Prérequis

### Logiciels nécessaires

Assurez-vous que les outils suivants sont installés sur votre système hôte :

- `docker`
- `parted`
- `losetup`
- `mkfs.ext4`
- `qemu-system-x86_64`
- `grub-install`

### Fichiers requis

- Une image de noyau Linux précompilée (par défaut : `../linux-6.12.3/arch/x86/boot/bzImage`).
- Un module rootkit précompilé nommé `rootkit.ko` dans le répertoire courant.

## Utilisation

### Étape 1 : Lancer le script

```bash
sudo ./script.sh
```

Ce script :

1. Crée une image disque de 450 Mo.
2. Partitionne et formate l'image en ext4.
3. Monte la partition et installe Alpine Linux minimal à l'aide de Docker.
4. Copie le noyau personnalisé et le module rootkit dans l'image.
5. Configure GRUB pour permettre le démarrage de l'image disque.
6. Ajoute un script d'initialisation pour insérer le module rootkit au démarrage.
7. Nettoie les ressources temporaires.
8. Démarre la VM avec QEMU.

### Étape 2 : Connexion à la VM

Une fois la VM démarrée, connectez-vous avec les identifiants suivants :

- **Utilisateur root**
  - Nom d'utilisateur : `root`
  - Mot de passe : `rootpassword`
- **Utilisateur standard**
  - Nom d'utilisateur : `user`
  - Mot de passe : `userpassword`

### Étape 3 : Vérifier le rootkit

1. Connectez-vous en tant que `root`.
2. Vérifiez que le script de démarrage a bien inséré le module rootkit :
   ```bash
   lsmod | grep rootkit
   ```
   Si le module n'est pas chargé automatiquement, insérez-le manuellement :
   ```bash
   insmod /home/user/rootkit.ko
   ```
3. Vérifiez les logs du noyau pour des messages relatifs au rootkit :
   ```bash
   dmesg | grep rootkit
   ```

## Dépannage

### Problème : `/bin/sh` introuvable

Assurez-vous que le script de création d'image inclut correctement le lien symbolique vers `/bin/busybox` :

```bash
ln -sf /bin/busybox /sbin/init
```

### Problème : Le module rootkit ne peut pas être inséré

Vérifiez que :

- Le module rootkit a été compilé avec le même noyau que celui utilisé dans l'image.
- Les options de signature de module sont désactivées dans le noyau (via `module.sig_enforce=0` dans la configuration GRUB).
- Les dépendances du module sont satisfaites.

### Problème : La VM ne démarre pas

1. Vérifiez que le noyau est correctement copié dans l'image :
   ```bash
   ls $MOUNT_DIR/boot/vmlinuz
   ```
2. Assurez-vous que GRUB est installé correctement :
   ```bash
   grub-install --directory=/usr/lib/grub/i386-pc --boot-directory=$MOUNT_DIR/boot $LOOP_DEVICE
   ```

## Structure du Projet

- `script.sh`: Script principal pour créer et démarrer l'image.
- `rootkit.ko`: Module rootkit à insérer dans la VM.
- `disk.img`: Image disque générée par le script.

## Configuration GRUB

Voici la configuration GRUB utilisée dans ce projet :

```grub
serial
terminal_input serial
terminal_output serial
set root=(hd0,1)
menuentry "Linux2600" {
    linux /boot/vmlinuz root=/dev/sda1 console=ttyS0 module.sig_enforce=0 init=/sbin/init
}
```

## Avertissements

- Ce projet est à but éducatif uniquement. Ne l'utilisez pas sur des systèmes en production ou sans autorisation.
- Manipuler des rootkits peut entraîner des risques pour la sécurité et la stabilité du système. Soyez prudent.

