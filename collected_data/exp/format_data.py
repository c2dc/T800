import os
import re
from functools import reduce

RAW_FILES_DIR = './raw_data'
FORMATED_FILES_DIR = './formated_data'

if __name__ == '__main__':
    raw_files = os.listdir(RAW_FILES_DIR)
    for file_name in raw_files:
        print(f'processing {file_name}')
        with open(f'{RAW_FILES_DIR}/{file_name}', 'r') as in_file:
            header_pattern = r'[A-Za-z_\s]+[A-Za-z_,0-9\s]+\n[^a-zA-Z_]+'
            csvs = [re.sub('Tmr Svc', 'TmrSvc', csv.strip('\n ')) for csv in re.findall(header_pattern, in_file.read())]
        for i, csv in enumerate(csvs):
            print(f'\twriting {file_name}_{i}')
            with open(f'{FORMATED_FILES_DIR}/{file_name[:-4]}_{i}.csv', 'w') as out_file:
                out_file.write(csv)