import sys
import re
from collections import namedtuple
from enum import Enum
from pathlib import Path
from typing import Dict, List, Tuple

import sbn


import threading
lock = threading.Lock()


class Variable(Enum):
    D=0
    I=1
    J=2
    K=3
    W=4

Spectrum_file = namedtuple('Spectrum_file', 'filename, station, var, year')


class File_kernel(sbn.Kernel):

    def __init__(self, file: Spectrum_file):
        super(File_kernel, self).__init__()
        self._file = file

    def act(self):
        print(self._file)
        sbn.commit(self)


class Five_files_kernel(sbn.Kernel):

    def __init__(self, files: List[Spectrum_file]):
        super(Five_files_kernel, self).__init__()
        self._files = files
        self._count = len(self._files)
        
    def act(self):
        for file in self._files:
            sbn.upstream(self, File_kernel(file))

    def react(self, child: File_kernel):
        #lock.acquire()
        self._count -= 1
        #gitlock.release()
        if self._count == 0: sbn.commit(self)
        


class Spectrum_directory_kernel(sbn.Kernel):
    
    def __init__(self, input_directories: List[str]):
        super(Spectrum_directory_kernel, self).__init__()
        self._input_directories = input_directories


    def _complete(self, files: List[Spectrum_file]) -> bool:
        '''Checking that all the necessary files are available'''
        return len(files) == 5 and len(set(f.var for f in files)) == 5


    def act(self):
        print('>>>> Python (spec): spectrum-directory %i' % len(self._input_directories))

        # Grouping files
        files = {}
        pattern = re.compile(r"(\d+)([dijkw])(\d+).txt.gz")
        for input_dir in self._input_directories:
            for path_file in Path(input_dir).glob('**/*'):
                if match:=re.match(pattern, path_file.name):
                    station, variable, year = match.groups()
                    print('>>>> Python (spec): file %s station %s variable %s year %s' %\
                        (path_file, station, variable, year))
                    if (year, station) not in files: files[(year, station)] = []
                    files[(year, station)].append(Spectrum_file(
                        filename=path_file,
                        station=int(station),
                        var=Variable[variable.upper()],
                        year=int(year)))

        # skip incomplete records that have less than five files
        files = dict(filter(lambda item: self._complete(item[1]), files.items()))

        if len(files) == 0: sbn.commit(self)

        # Processing groups of files
        self._num_kernels = len(files)
        for item in files.items():
            sbn.upstream(self, Five_files_kernel(item[1].copy()))


    def react(self, child: Five_files_kernel):
        sbn.commit(self)
    

class Main(sbn.Kernel):

    def __init__(self, *args, **kwargs):
        super(Main, self).__init__(*args, **kwargs)
        self._input_directories = sys.argv[1:].copy()

    def act(self):
        print('>>>> Python (spec): program start!')
        sbn.upstream(self, Spectrum_directory_kernel(self._input_directories.copy()))

    def react(self, child: sbn.Kernel):
        print('>>>> Python (spec): finished all!')
        sbn.commit(self)
