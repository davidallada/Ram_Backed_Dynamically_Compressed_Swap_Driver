rdnr=1
rdsize=50
maxpart=1
partcompressed=25
lowwatermarkparam=50
highwatermarkparam=99

while getopts 's:p:l:h:?' OPTION; do
    case "$OPTION" in
        s)
            rdsize=$OPTARG
            echo "rd_size set to $rdsize"
            ;;
        p)
            partcompressed=$OPTARG
            echo "part_compressed set to $partcompressed"
            ;;
        l)
            lowwatermarkparam=$OPTARG
            echo "low_watermark_param set to $lowwatermarkparam"
            ;;
        h)
            highwatermarkparam=$OPTARG
            echo "highwatermarkparam set to $highwatermarkparam"
            ;;
        ?)
            echo "Usage: $(basename $0) [-s rd_size] [-p part_compressed] [-l low_watermark_param] [-h high_watermark_param]"
            exit 1
            ;;
    esac
done

let rdsize=$rdsize*1000

echo "Running with params: rd_nr=$rdnr rd_size=$rdsize max_part=$maxpart part_compressed=$partcompressed low_watermark_param=$lowwatermarkparam high_watermark_parm=$highwatermarkparam"

sudo rmmod ram.ko
sudo rmmod rle_simple_module.ko
make clean

if \
    make && \
    sudo insmod rle_simple_module.ko && \
    sudo insmod ram.ko \
        rd_nr=$rdnr \
        rd_size=$rdsize \
        max_part=$maxpart \
        part_compressed=$partcompressed \
        low_watermark_param=$lowwatermarkparam \
        high_watermark_parm=$highwatermarkparam;
then 
    echo "Successfully compiled and installed ram driver"
else
    echo "Failed to install ram driver"
    exit 1
fi

echo "[+] Turning off swap"
sudo swapoff -a
echo "[+] Creating Swap partition for ram0 in fdisk"
echo "n 






w" | sudo fdisk /dev/ram0
echo "[+] re-reading partition table"
partprobe
echo "[+] Checking /etc/fstab for existing swap"
str1=`tail -n 1 /etc/fstab | grep '/dev/ram0p1'`
str2="/dev/ram0p1 swap swap defaults,pri=3 0 0"
echo $str1
echo $str2
if [ "$str1" == "$str2" ]; then
    echo "[+] ram0p1 swap already in fstab"
else
    echo "[+] Adding ram0p1 swap to fstab"
    sudo sh -c "echo $str2 >> /etc/fstab"
fi
sudo mkswap /dev/ram0p1
sudo swapon -a

