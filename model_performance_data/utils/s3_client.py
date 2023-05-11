from botocore.exceptions import ClientError
import os.path
import boto3
import sys
from pathlib import Path
import utils.storage_connector as sc

class S3_Client:

    def __init__(self, cred):

        self.disable_insecure_connection_warnings(cred)
        self.connect(cred)
        self.none_variations = ['none', 'None', None, ['None'], [None], 'null', 'Null', ['Null'], ['null'], []]

    def connect(self, cred):

        self.client = boto3.client('s3', **cred)

    def disable_insecure_connection_warnings(self, cred):

        if ("verify", False) in cred.items():
            import urllib3
            urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)
            print("disable_insecure_connection_warnings has been operated")
        else:
            pass

    def get_object(self, bucket, key):
        return self.client.get_object(Bucket=bucket, Key=key)

    def put_object(self, bucket, key, body):
        self.client.put_object(Bucket=bucket, Key=key, Body=body)


    def get_files_count(self, bucket, folder):
        
        files = self.get_files_in_folder(bucket, folder)
        files_location = sc.get_files_location(files)
        location_data = sc.get_files_suffix(files_location)
        return location_data

    def count_files(self, bucket: str, folder: str, extension: str = None,
                    word_must: str = None, word_ignore: str = None):
        files = self.get_files_in_folder(bucket=bucket,
                                         folder=folder,
                                         suffix=extension,
                                         string_must=word_must,
                                         string_ignore=word_ignore)
        return len(files)

    def get_file_objects(self, bucket, folder):
        paginator = self.client.get_paginator('list_objects_v2')
        pages = paginator.paginate(Bucket=bucket, Prefix=folder)
        return pages

    def get_all_final_folders(self, bucket, folder, strings_to_exclude=[], strings_to_include=None):

        paginator = self.client.get_paginator('list_objects')
        loop_folder = [folder]
        last_folder = False
        while not last_folder:
            folder_list = []
            for folder in loop_folder:
                ignore_folder = False
                for ignore in strings_to_exclude:
                    if ignore in folder:
                        ignore_folder=True
                if not ignore_folder:
                    sub_folder = self.get_sub_folder(paginator, bucket, folder, strings_to_include)
                    if sub_folder == [folder]:
                        folder_list = loop_folder
                        break
                    folder_list.extend(sub_folder)
            if folder_list != loop_folder:
                loop_folder = folder_list
            else:
                last_folder = True
        if strings_to_include is not None:
            folder_list_with_string = []
            for folder in folder_list:
                if strings_to_include in folder:
                    folder_list_with_string.append(folder)
            return folder_list_with_string
        return folder_list

    def get_sub_folder(self, paginator, bucket, folder, strings_to_include=None):
        all_folders = []
        for result in paginator.paginate(Bucket=bucket, Prefix=folder, Delimiter='/'):
            if result.get('CommonPrefixes') != None:
                for prefix in result.get('CommonPrefixes'):
                    fetched_prefix = prefix.get('Prefix')
                    if prefix.get('Prefix') not in all_folders:
                        if strings_to_include in self.none_variations or any(self.is_substring_in_string(strings_to_include, fetched_prefix)):
                            all_folders.append(prefix.get('Prefix'))
            elif folder not in all_folders:
                if strings_to_include in self.none_variations or any(self.is_substring_in_string(strings_to_include, folder)):
                    all_folders.append(folder)

        return all_folders

    def get_info_from_pages(self, bucket, folder):
        pages = self.get_file_objects(bucket, folder)
        info_file = []
        for response in pages:
            if 'Contents' in response:
                for content in response['Contents']:
                    info_file.append(content)

        return info_file

    def copy_obj(self, dest_bucket, source, key):
        response = self.client.copy_object(Bucket=dest_bucket, CopySource=source, Key=key)
        return response

    def delete_folder(self, bucket, folder, filename_length=None):

        files = self.get_files_in_folder(bucket, folder)
        for file in files:
            if filename_length is not None and len(file.rsplit('/', 1)[1].split(".")[0]) == filename_length:
                response = self.client.delete_object(
                    Bucket=bucket,
                    Key=file
                )
            elif filename_length is None:
                print(file)
                response = self.client.delete_object(
                    Bucket=bucket,
                    Key=file
                )
            print(file)
            response = self.client.delete_object(
                Bucket=bucket,
                Key=file
            )

    def create_folder(self, bucket, folder):

        response = self.client.put_object(Bucket=bucket, Key=(folder + '/'))
        return response

    def get_files_in_folder(self, bucket, folder, suffix=None, string_ignore=None, string_must=None):
       
        pages = self.get_file_objects(bucket, folder)
        files_location = []
        for response in pages:
            if 'Contents' in response:
                for content in response['Contents']:
                    if (not content['Key'].endswith('/')) and (suffix in self.none_variations or content['Key'].endswith(suffix)) \
                            and (string_must in self.none_variations or string_must in content['Key']) \
                            and (string_ignore in self.none_variations or string_ignore not in content['Key']):
                        files_location.append(content['Key'])
        return files_location

    def get_files_as_dict(self, bucket, folder, suffix=None, strings_to_exclude=None, strings_to_include=None,
                          exclude_all=False, include_all=False):
        """
        The same as get_files_in_folder, creating a dictionary with the following structure:
        S3_folder : list_of_objects
        Parameters:
        * bucket: S3 bucket (String)
        * folder: The folder in S3 to search in (String)
        * suffix: the suffix of the files you want to include in your query (String)
        * strings_to_exclude: Decide on one or multiple strings that, if they exist in the object prefix, the object will be ignored. (String / List of Strings)
        * strings_to_include: Decide on one or multiple strings that, if they exist in the object prefix, the object will be included in the query. (String / List of Strings)
        * exclude_all: True if you want that an object will be ignored in query only if all strings_to_exclude exist in the object prefix,
                        False if you want that an object will be ignored in query even if only one of the strings_to_exclude exist in the object prefix
       * exclude_all: True if you want that an object will be included in query only if all strings_to_include exist in the object prefix,
                        False if you want that an object will be included in query even if only one of the strings_to_include exist in the object prefix.
        @return: Dictionary of folder: frames list

        """
        pages = self.get_file_objects(bucket, folder)
        folder_files_dict = {}

        if not isinstance(strings_to_exclude, list):
            strings_to_exclude = [strings_to_exclude]
        if not isinstance(strings_to_include, list):
            strings_to_include = [strings_to_include]

        include_function = all if include_all == True else any
        exclude_function = all if exclude_all == True else any

        for response in pages:
            if 'Contents' in response:
                for content in response['Contents']:
                    key = content['Key']
                    if suffix in self.none_variations or suffix in key:
                        if strings_to_include in self.none_variations or include_function((self.is_substring_in_string(strings_to_include, key))):
                            if strings_to_exclude in self.none_variations or not exclude_function(self.is_substring_in_string(strings_to_exclude, key)):
                                folder_path = os.path.dirname(key)
                                if folder_path not in folder_files_dict:
                                    folder_files_dict[folder_path] = [key]
                                else:
                                    folder_files_dict[folder_path].append(key)
        return folder_files_dict

    def is_substring_in_string(self, list, string):

            return [substring.casefold() in string.casefold() for substring in list]

    def get_files_parameters(self, bucket, folder, suffix=None, parameter='Size', strings_to_exclude=None,
                             strings_to_include=None):

        """
        Utilizes S3 Paginator to return a dict of - S3 object key : S3 object parameter.
        Parameters like prefix, last modified, storageclass, ETag (a MD5 hash of the object - doesn't work for big, multipart uploaded files).
        The full list of optional parameters is here:
        https://boto3.amazonaws.com/v1/documentation/api/latest/reference/services/s3.html#S3.Client.list_objects_v2
        The default parameter is the object filesize.
        
        returns a dict of S3 object key : S3 object parameter
        """

        file_dict = {}
        files = self.get_info_from_pages(bucket, folder)
        for file in files:
            key = file['Key']
            if suffix in self.none_variations or key.endswith(suffix):
                if strings_to_exclude in self.none_variations or not any(self.is_substring_in_string(list=strings_to_exclude, string=key)):
                    if strings_to_include in self.none_variations or any(self.is_substring_in_string(list=strings_to_include, string=key)):
                        file_dict[key] = file[parameter]

        return file_dict

    def download_file(self, bucket, key, filepath):
        self.client.download_file(bucket, key, filepath)

    def upload_file(self, filepath, bucket, s3_key):
        self.client.upload_file(filepath, bucket, s3_key)

    def file_exists(self, bucket, key):

        try:
            self.client.head_object(Bucket=bucket, Key=key)
            return True
        except ClientError:
            return False

    def change_name(self, bucket, folder, df):

        folders = self.get_files_as_dict(bucket, folder)
        df = df.reset_index()  # make sure indexes pair with number of rows
        count_folders=0

        for index, row in df.iterrows():
            old_name = row['Old Name']
            for folder in folders:
                count_frame=1
                if old_name in folder:
                    count_folders += 1
                    print(folder)
                    for file in folders[folder]:
                        response = self.copy_obj(bucket, f"{bucket}/{folder.split('/',1)[0]}/{row['Old Name']}/{file.rsplit('/',1)[1]}", f"{folder.split('/',1)[0]}/{row['New Name']}/{file.rsplit('/',1)[1]}")
                        if response['ResponseMetadata']['HTTPStatusCode'] != 200:
                            print(f"failed for {file}")
                        count_frame+=1
                    print(count_frame)
        print(count_folders)

    def check_frame_names(self, s3_folder_files_dict):

        """ returns an errors dict in the structure of:
            {key : dict,
             key: {key: list, key: list, key:list}}"""

        errors = {'folders_with_index_errors' : {},
                  "frame_name_errors" :
                      {"index_longer_than_5_chars" : [], "index_shorter_than_5_chars" : [], "file_index_is_not_a_digit" : []}
                  }

        for folder, files in s3_folder_files_dict.items():
            frame_indices = []

            for file in files:
                frame_index, frame_name_error = self.frame_name_validity_test(file)
                frame_indices.append(frame_index)
                if frame_name_error:
                    errors['frame_name_errors'][frame_name_error].append(file)

            frame_indices.sort()
            index = 1
            for frame_index in frame_indices:
                if frame_index.isdigit():
                    if int(frame_index) != index:
                        errors['folders_with_index_errors'][folder] = index
                        break
                    index += 1

        return errors

    def frame_name_validity_test(self, frame_name):

        error = None
        frame_index = frame_name.rsplit('/', 1)[1].split('.')[0]

        if not frame_index.isdigit():
            error = 'file_index_is_not_a_digit'

        elif len(frame_index) > 5:
            frame_index = frame_index[-5:] # the "-:5" takes the last 5 characters of the image index, in order to deal with cases where the image name contains a prefix, such as in REID dataset (see prai_reid and vrai_reid for example)
            error = "index_longer_than_5_chars"

        elif len(frame_index) < 5:
            error = "index_shorter_than_5_chars"

        return frame_index, error
