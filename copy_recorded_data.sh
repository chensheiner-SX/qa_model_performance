#!/bin/bash


###############################
# ABOUT:
# Script for copy data from NX
# to local PC and delete the 
# recorded data on the NX
###############################

$IP
$DIR

if [[ $# < 4 ]]; then
	printf "Syntax Error. please run $0 -ip <NX_ip> -d <path_to_save>\n"
	printf "Example: $0 -ip 172.12.10.33 -d /home/${USER}/Desktop/\n"
	exit 1
fi

while [ $# != 0 ]; do
	case "$1" in
		-ip)
			IP=$2
			shift 2
			;;
		-d)
			if [ $2 = "." ]; then
				DIR=$(pwd)
			else
				DIR=$2
			fi
			shift 2
			;;
		*)
			echo "Invalid option, exiting ..."
			exit 1
			;;
	esac
done

#check if provided dir argument is exist
if [ ! -d $DIR ]; then
	echo "$DIR is invalid, please try again..."
	exit 1
fi

#getting the recording folder name and checks if its empty	
dir_name=$(ssh teddybear@$IP "cd /var/sightx/.docker_output/sightx_debug && ls -ltr | tail -1 | cut -d ' ' -f 9")

if [[ -z $dir_name ]]; then
	printf "Recording folder in the NX is empty, exiting...\n"
	exit 1
fi

#copy and clean from NX
scp -r teddybear@$IP:/var/sightx/.docker_output/sightx_debug/$dir_name $DIR
ssh -t teddybear@$IP "sudo rm -rf /var/sightx/.docker_output/sightx_debug/*"

echo "Recorded data copied to $DIR into folder $dir_name and deleted from NX $IP"
exit 0 

