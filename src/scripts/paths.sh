SCRIPTS_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
SSD_MNT_="/home/`whoami`/mnt/ssd/"
FUSE_MNT_="/home/`whoami`/mnt/fuse/"
SSD_MNT="/home/`whoami`/mnt/ssd"
FUSE_MNT="/home/`whoami`/mnt/fuse"
SSD_DISK="/home/`whoami`/ssd_image/ssd_image.disk"
CLOUDFS_BIN="$SCRIPTS_DIR/../build/cloudfs"
CACHE_DIR="$SSD_MNT/.cache"
S3_DIR="/tmp/s3"
