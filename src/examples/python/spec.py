import os, sys
import signal
import re
import gzip
import time
from math import pi, cos
# from datetime import datetime
from collections import namedtuple
from enum import Enum
from pathlib import Path
from typing import Dict, List, Optional

import sbn


class Variable(Enum):
    D = 0
    I = 1
    J = 2
    K = 3
    W = 4


DENSITY = Variable.W.value
ALPHA_1 = Variable.D.value
ALPHA_2 = Variable.I.value
R_1     = Variable.J.value
R_2     = Variable.K.value

Spectrum_file = namedtuple('Spectrum_file', 'filename, station, var, year')


def datetime_to_timestamp(
    year: int = 0, month: int = 0, day: int = 0, hour: int = 0, minute: int = 0, second: int = 0) -> int:
    return second + 60 * (minute + 60 * (hour + 24 * (day + 31 * (month + 12 * year))))


class Variance_kernel(sbn.Kernel):

    def __init__(self,
        var_values: List[List[float]]=None,
        timestamp: int=None,
        freq: List[float]=None
        ):
        super(Variance_kernel, self).__init__()
        self._data = var_values
        self._date = timestamp
        self._frequencies = freq
        self._variance = 0

    def _spectrum(self, i: int, angle: float) -> float:
        density = self._data[DENSITY][i]
        r1 = self._data[R_1][i]
        r2 = self._data[R_2][i]
        alpha1 = self._data[ALPHA_1][i]
        alpha2 = self._data[ALPHA_2][i]
        return density * (1/pi) * (0.5
            + 0.01 * r1 * cos(     angle - alpha1)
            + 0.01 * r2 * cos(2 * (angle - alpha2)))

    def _compute_variance(self) -> float:
        theta0 = 0
        theta1 = 2.0 * pi
        n = min(len(self._frequencies), min(map(len, self._data)))
        return sum(
            self._spectrum(i, theta0 + (theta1 - theta0)*j/n)
            for i in range(n) for j in range(n))

    def act(self):
        self._variance = self._compute_variance()
        sbn.commit(self, target=sbn.Target.Local)

    def variance(self) -> float: return self._variance

    def date(self) -> int: return self._date


class File_kernel(sbn.Kernel):

    def __init__(self, sfile: Spectrum_file=None):
        super(File_kernel, self).__init__()
        self._sfile = sfile
        self._frequencies = []
        self._data = {}

    def act(self):
        if hostname := os.getenv("SBN_TEST_SUBORDINATE_FAILURE"):
            if os.uname()[1] == hostname:
                print('>>>> Python (spec): simulate subordinate failure %s' % hostname, file=open('pyspec.log', 'a'))
                os.kill(os.getppid(), signal.SIGKILL)
                os.kill(os.getpid(), signal.SIGKILL)

        # filename = Path('/var').joinpath(self._sfile.filename)  # TODO
        with gzip.open(self._sfile.filename, 'rb') as file_obj:
            num_lines = 0
            # if the file is empty, there will be no iterations
            for line in file_obj:
                line = line.decode("utf-8")
                # read frequencies
                if line[0] == '#':
                    if len(self._frequencies) == 0:
                        self._frequencies = list(map(float, line[16:].split()))
                    # else skip lines starting with "#"
                else:
                    # YYYY MM DD hh mm
                    # 2010 01 01 00 00 -> time_componets
                    *time_componets, line = line.split(maxsplit=5)

                    # timestamp = datetime(*map(int, time_componets)).timestamp()
                    timestamp = datetime_to_timestamp(*map(int, time_componets))

                    # read spec values
                    map_func = lambda val: 0 if abs(float(val) - 999) < 1e-1 else float(val)  # missing values
                    self._data[timestamp] = list(map(map_func, line.split()))

                    num_lines += 1


        print('>>>> Python (spec): %s: %i records %i lines' % \
            (self._sfile.filename, len(self._data), num_lines), file=open('pyspec.log', 'a'))
        sbn.commit(self, target=sbn.Target.Local)

    def data(self) -> Dict[int, List[float]]: return self._data

    def sfile(self) -> Spectrum_file: return self._sfile

    def frequencies(self) -> List[float]: return self._frequencies


class Five_files_kernel(sbn.Kernel):

    class State(Enum):
        Reading = 0
        Processing = 1

    def __init__(self, files: List[Spectrum_file]=None):
        super(Five_files_kernel, self).__init__()
        self._files = files
        self._frequencies = None
        self._count = 0
        self._spectra = {}
        self._out_matrix = {}
        self._state = self.State.Reading

    def act(self):
        for file in self._files:
            sbn.upstream(self, File_kernel(file), target=sbn.Target.Local)

    def _remove_incomplete_records(self) -> None:
        self._spectra = dict(filter(
            lambda item: all(
                len(item[1][0]) == len(var_values)
                for var_values in item[1][1:]),
            self._spectra.items()
        ))

    def _process_spectra(self) -> None:
        self._state = self.State.Processing
        print('>>>> Python (spec): %i records from station %i, year %i' %\
                (len(self._spectra), self.station(), self.year()), file=open('pyspec.log', 'a'))

        old_size = len(self._spectra)
        self._remove_incomplete_records()
        new_size = len(self._spectra)
        if new_size < old_size:
            print('>>>> Python (spec): removed %i incomplete records from station %i, year %i' %\
                (old_size - new_size, self.station(), self.year()), file=open('pyspec.log', 'a'))

        if new_size == 0: sbn.commit(self, target=sbn.Target.Remote)
        else:
            self._count = new_size
            for timestamp, var_values in self._spectra.items():
                sbn.upstream(self,
                    Variance_kernel(var_values, timestamp, self._frequencies),
                    target=sbn.Target.Local)

    def _add_spectrum_variable(self, child: File_kernel) -> None:
        self._count += 1

        var = child.sfile().var
        for timestamp, spec_values in child.data().items():
            if timestamp not in self._spectra:
                self._spectra[timestamp] = [ [] for _ in range(len(Variable)) ]
            self._spectra[timestamp][var.value] = spec_values

        if self._frequencies is None: self._frequencies = child.frequencies()

        if self._count == 5: self._process_spectra()

    def _add_variance(self, child: Variance_kernel) -> None:
        self._count -= 1

        self._out_matrix[child.date()] = child.variance()
        print('>>>> Python (spec): remaining %i' % (self._count))
        if self._count == 0:
            print('>>>> Python (spec): finished year %i station %i' % (self.year(), self.station()), file=open('pyspec.log', 'a'))
            sbn.commit(self, target=sbn.Target.Remote)

    def react(self, child):
        if self._state == self.State.Reading:
            self._add_spectrum_variable(child)
        elif self._state == self.State.Processing:
            self._add_variance(child)

    def write_output_to(self, outfile: Path) -> None:
        with open(outfile, 'a') as out:
            for variance in self._out_matrix.values():
                out.write('%i,%i,%f\n' % (self.year(), self.station(), variance))

    def year(self) -> Optional[int]:
        return self._files[0].year if len(self._files) != 0 else None

    def station(self) -> Optional[int]:
        return self._files[0].station if len(self._files) != 0 else None

    def num_processed_spectra(self) -> int: return len(self._out_matrix)

    def sum_processed_spectra(self) -> float: return sum(self._out_matrix.values())


class Spectrum_directory_kernel(sbn.Kernel):

    def __init__(self, input_directories: List[str]=None):
        super(Spectrum_directory_kernel, self).__init__()
        self._input_directories = input_directories
        self._count = 0
        self._num_kernels = 0
        self._count_spectra = 0
        self._sum_spectra = 0

    def _complete(self, files: List[Spectrum_file]) -> bool:
        '''Checking that all the necessary files are available'''
        return len(files) == 5 and len(set(f.var for f in files)) == 5

    def act(self):
        if hostname := os.getenv("SBN_TEST_SUPERIOR_COPY_FAILURE"):
            if os.uname()[1] == hostname:
                if str_seconds := os.getenv("SBN_TEST_SLEEP_FOR"):
                    seconds = int(str_seconds)
                    print('>>>> Python (spec): sleeping for %i seconds' % seconds, file=open('pyspec.log', 'a'))
                    sbn.sleep(int(seconds * 1e+3))
                print('>>>> Python (spec): simulate superior copy failure %s' % hostname, file=open('pyspec.log', 'a'))
                os.kill(os.getppid(), signal.SIGKILL)
                os.kill(os.getpid(), signal.SIGKILL)

        print('>>>> Python (spec): spectrum-directory %i' % len(self._input_directories), file=open('pyspec.log', 'a'))

        # Grouping files
        files = {}
        pattern = re.compile(r"(\d+)([dijkw])(\d+).txt.gz")
        for input_dir in self._input_directories:
            for path_file in Path(input_dir).glob('**/*'):
                if match := re.match(pattern, path_file.name):
                    station, variable, year = match.groups()
                    print('>>>> Python (spec): file %s station %s variable %s year %s' % \
                        (path_file, station, variable, year), file=open('pyspec.log', 'a'))
                    if (year, station) not in files: files[(year, station)] = []
                    files[(year, station)].append(Spectrum_file(
                        filename=path_file,
                        station=int(station),
                        var=Variable[variable.upper()],
                        year=int(year)))

        # skip incomplete records that have less than five files
        files = dict(filter(lambda item: self._complete(item[1]), files.items()))

        if len(files) == 0: sbn.commit(self, target=sbn.Target.Remote)
        else:
            # Processing groups of files
            self._num_kernels = len(files)
            for item in files.items():
                sbn.upstream(self, Five_files_kernel(item[1].copy()), target=sbn.Target.Remote)

    def react(self, child: Five_files_kernel):
        self._count += 1

        year = child.year()
        output_file = Path(str(year) + '_pyspec.out')
        child.write_output_to(output_file)

        self._count_spectra += child.num_processed_spectra()
        self._sum_spectra += child.sum_processed_spectra()

        print('>>>> Python (spec): [%i/%i] finished station %i, year %i, total no. of spectra %i' % (
            self._count, self._num_kernels,
            child.station(), child.year(), child.num_processed_spectra()), file=open('pyspec.log', 'a'))

        if self._count == self._num_kernels:
            print('>>>> Python (spec): total number of processed spectra %i' % self._count_spectra, file=open('pyspec.log', 'a'))
            with open('nspectra.log', 'a') as log: log.write('%i\n' % self._count_spectra)
            print('>>>> Python (spec): total sum of processed spectra %i' % self._sum_spectra, file=open('pyspec.log', 'a'))
            with open('sumspectra.log', 'a') as log: log.write('%i\n' % self._sum_spectra)
            sbn.commit(self, target=sbn.Target.Remote)


class Main(sbn.Kernel):

    def __init__(self, *args, **kwargs):
        super(Main, self).__init__(*args, **kwargs)
        self._input_directories = sys.argv[1:].copy()
        self._time_start = 0
        self._time_end = 0

    def act(self):
        self._time_start = time.time()
        print('>>>> Python (spec): ============= program start! =============', file=open('pyspec.log', 'a'))
        k = Spectrum_directory_kernel(self._input_directories.copy())
        k.enable_carries_parent()  # carries_parent
        sbn.upstream(self, k, target=sbn.Target.Remote)

        if hostname := os.getenv("SBN_TEST_SUPERIOR_FAILURE"):
            if os.uname()[1] == hostname:
                sbn.sleep(100)  # 100 milliseconds
                print('>>>> Python (spec): simulate superior failure %s' % hostname, file=open('pyspec.log', 'a'))
                os.kill(os.getppid(), signal.SIGKILL)
                os.kill(os.getpid(), signal.SIGKILL)

    def react(self, child: sbn.Kernel):
        self._time_end = time.time()
        total_time = int((self._time_end - self._time_start) * 1e+6)
        print('>>>> Python (spec): finished all!', file=open('pyspec.log', 'a'))
        print('>>>> Python (spec): total time execution: %i us' % total_time, file=open('pyspec.log', 'a'))
        with open('time.log', 'a') as log: log.write('%i\n' % total_time)
        sbn.commit(self, target=sbn.Target.Remote)
