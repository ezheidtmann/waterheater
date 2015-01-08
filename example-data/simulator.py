#!/usr/bin/env python

# This pulse file is expected to contain one pulse, starting and ending at
# zero. Any padding before or after the pulse will contribute to errors or
# confusion.
VALID_PULSE_FILE='valid-pulse.csv'

# This pulse file can contain anything. It will be randomly overlaid on the
# simulated output.
NOISE_PULSE_FILE='noise-pulse.csv'

# x (ms) between output rows
OUTPUT_SPACING=0.5

# Chance that any non-noisy millisecond will start a noise pulse
NOISE_PROBABILITY=0.001

# Highest voltage allowable
MAX_Y=5

############################

import csv
from random import random
from scipy.interpolate import interp1d

def get_wave_data_dict(filename):
    with open(filename) as csvfile:
        reader = csv.DictReader(csvfile, fieldnames=('x', 'y'))
        ret = { 'x': [], 'y': [] }
        x_offset = 0
        for row in reader:
            try:
                x = float(row['x'])
                y = float(row['y'])
            except ValueError:
                continue

            # First numerical row, sets x offset. This ensures that our
            # returned vectors start at zero.
            if len(ret['x']) == 0:
                x_offset = x;

            ret['x'].append(x - x_offset)
            ret['y'].append(y)

        return ret

def make_simple_wave(data, **kwargs):
    kwargs.setdefault('kind', 'cubic')
    wave = interp1d(data['x'], data['y'], **kwargs)
    wave.max_x = max(data['x'])
    return wave

def make_repeating_wave(wave, offset=0, gap=0, floor=0):
    """
    For good behavior, please meet assumptions:
     - data is a dict with two keys: 'x' and 'y', both lists (vectors)
     - 'x' vectors starts at 0
     - if gap is specified, 'y' vector starts and ends at floor (default=0)
    """

    known_length = wave.max_x
    cycle_length = offset + known_length + gap

    def thewave(x):
        # get x within our supported range
        x = x % cycle_length
        if x < offset:
            return floor
        elif x < offset + known_length:
            return wave(x)
        else:
            return floor

    return thewave

valid_single = make_simple_wave(get_wave_data_dict(VALID_PULSE_FILE))
valid_3seconds = make_repeating_wave(valid_single, gap=3000)

noise_single = make_simple_wave(get_wave_data_dict(NOISE_PULSE_FILE), kind='linear')

### the big loop

noise_is_active = False
noise_started = 0
active_noise = None
x = 0
while True:
    y = float(valid_3seconds(x))

    if not noise_is_active and random() < NOISE_PROBABILITY * OUTPUT_SPACING:
        active_noise = noise_single
        noise_is_active = True
        noise_started = x

    if noise_is_active:
        noise_x = x - noise_started
        if noise_x <= active_noise.max_x:
            y += active_noise(x - noise_started)
        else:
            noise_is_active = False

    # Cap output at maximum allowed by circuitry
    y = min(y, MAX_Y)

    try:
        print "{x:.3f},{y:.3f}".format(x=x, y=y)
    except IOError:
        break

    x += float(OUTPUT_SPACING)
