import sys
import re
import gzip
from math import pi, cos
from datetime import datetime
from collections import namedtuple
from enum import Enum
from pathlib import Path
from typing import Dict, List, TextIO, Optional

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


class Variance_kernel(sbn.Kernel):

    def __init__(self, 
        var_values: List[List[float]],
        timestamp: datetime.timestamp,
        freq: List[float]
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
        sbn.commit(self)

    def variance(self) -> float: return self._variance

    def date(self) -> datetime.timestamp: return self._date


class File_kernel(sbn.Kernel):

    def __init__(self, sfile: Spectrum_file):
        super(File_kernel, self).__init__()
        self._sfile = sfile
        self._frequencies = []
        self._data = {}

    def act(self):
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
                    timestamp = datetime(*map(int, time_componets)).timestamp()

                    # read spec values
                    # map_func = lambda val: 0 if float(val) - 999 < 1e-1 else float(val)  # TODO
                    self._data[timestamp] = list(map(float, line.split()))

                    num_lines += 1

        print('>>>> Python (spec): %s: %i records %i lines' % \
            (self._sfile.filename, len(self._data), num_lines))
        sbn.commit(self)  # TODO Local

    def data(self) -> Dict[datetime.timestamp, List[float]]: return self._data

    def sfile(self) -> Spectrum_file: return self._sfile

    def frequencies(self) -> List[float]: return self._frequencies


class Five_files_kernel(sbn.Kernel):

    class State(Enum):
        Reading = 0
        Processing = 1

    def __init__(self, files: List[Spectrum_file]):
        super(Five_files_kernel, self).__init__()
        self._files = files
        self._frequencies = None
        self._count = 0
        self._spectra = {}
        self._out_matrix = {}
        self._state = self.State.Reading
        
    def act(self):
        for file in self._files:
            sbn.upstream(self, File_kernel(file))  # TODO Local!

    def _remove_incomplete_records(self) -> None:
        self._spectra = dict(filter(
            lambda item: item[1][0] is not None and all(
                var_values is not None and len(item[1][0]) == len(var_values)
                for var_values in item[1]),
            self._spectra.items()
        ))

    def _process_spectra(self) -> None:
        self._state = self.State.Processing

        old_size = len(self._spectra)
        self._remove_incomplete_records()
        new_size = len(self._spectra)
        if new_size < old_size:
            print('>>>> Python (spec): removed %i incomplete records from station %i, year %i' %\
                (old_size - new_size, self.station(), self.year()))

        if new_size == 0: sbn.commit(self)
        else:
            self._count = new_size
            for timestamp, var_values in self._spectra.items():
                sbn.upstream(self,
                    Variance_kernel(var_values, timestamp, self._frequencies))

    def _add_spectrum_variable(self, child: File_kernel) -> None:
        self._count += 1

        var = child.sfile().var
        for timestamp, spec_values in child.data().items():
            if timestamp not in self._spectra:
                self._spectra[timestamp] = [None] * len(Variable)
            self._spectra[timestamp][var.value] = spec_values

        if self._frequencies is None: self._frequencies = child.frequencies()

        if self._count == 5: self._process_spectra()

    def _add_variance(self, child: Variance_kernel) -> None:
        self._count -= 1

        self._out_matrix[child.date()] = child.variance()
        if self._count == 0:
            print('>>>> Python (spec): finished year %i station %i' % (self.year(), self.station()))
            sbn.commit(self)

    def react(self, child):
        if self._state == self.State.Reading:
            self._add_spectrum_variable(child)
        elif self._state == self.State.Processing:
            self._add_variance(child)

    def write_output_to(self, out: TextIO) -> None:
        for variance in self._out_matrix.values():
            out.write('%i,%i,%f\n' % (self.year(), self.station(), variance))

    def year(self) -> Optional[int]:
        return self._files[0].year if len(self._files) != 0 else None

    def station(self) -> Optional[int]:
        return self._files[0].station if len(self._files) != 0 else None

    def num_processed_spectra(self) -> int: return len(self._out_matrix)


class Spectrum_directory_kernel(sbn.Kernel):

    def __init__(self, input_directories: List[str]):
        super(Spectrum_directory_kernel, self).__init__()
        self._input_directories = input_directories
        self._count = 0
        self._num_kernels = 0
        self._count_spectra = 0
        self._output_files = {}

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
                if match := re.match(pattern, path_file.name):
                    station, variable, year = match.groups()
                    print('>>>> Python (spec): file %s station %s variable %s year %s' % \
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
        else:
            # Processing groups of files
            self._num_kernels = len(files)
            for item in files.items():
                sbn.upstream(self, Five_files_kernel(item[1].copy()))

    def react(self, child: Five_files_kernel):
        self._count += 1

        year = child.year()
        if year not in self._output_files:
            self._output_files[year] = open(str(year) + '.out', 'a')
        output_file = self._output_files[year]
        child.write_output_to(output_file)

        self._count_spectra += child.num_processed_spectra()

        print('>>>> Python (spec): [%i/%i] finished station %i, year %i, total no. of spectra %i' % (
            self._count, self._num_kernels,
            child.station(), child.year(), child.num_processed_spectra()))

        if self._count == self._num_kernels: sbn.commit(self)


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
