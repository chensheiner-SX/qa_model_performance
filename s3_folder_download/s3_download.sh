########################
#Script for download folders from s3 bucket
# Written by Lior Lakay
#######################


if [[ $# != 2 ]]; then
	echo "Not enough arguments, please enter <s3_folder_path> <local_absulote_path>"
	exit 0
fi

aws s3 cp $1 $2 --recursive
