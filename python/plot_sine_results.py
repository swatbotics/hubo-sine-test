#!/usr/bin/env python

import sys
import numpy as np
import matplotlib.pyplot as plt

######################################################################

def diff(signal):
    return signal[1:] - signal[:-1]

def normalize(signal):
    meanshifted = signal - signal.mean()
    denom = np.linalg.norm(meanshifted)
    if not denom:
        return meanshifted
    else:
        return meanshifted / denom

def estimate_correlation(name1, signal1,
                         name2, signal2,
                         idx0, idx1, mean_dt,
                         min_shift = 0,
                         max_shift = 50):

    assert(len(signal1.shape) == 1)
    assert(len(signal2.shape) == 1)
    assert(len(signal1) == len(signal2))

    assert(idx0 - min_shift >= 0)
    assert(idx1 + max_shift <= len(signal1))
    assert(idx0 < idx1)
    assert(min_shift < max_shift)
    
    slice1 = normalize(signal1[idx0:idx1])

    num_shift = max_shift - min_shift
    results = np.zeros(num_shift)
    
    for i in range(num_shift):
        j = min_shift + i
        slice2 = normalize(signal2[idx0+j:idx1+j])
        results[i] = np.dot(slice1, slice2)
    
    best_shift = results.argmax()
    best_offset = best_shift + min_shift
    best_time = best_offset * mean_dt

    print 'max correllation between {0} and {1} is {2} at offset {3} (dt ~ {4})'.format(
        name1, name2, results[best_shift], best_offset, best_offset*mean_dt)

def check_skip(name, time, x, color, sz=None):
    dt = diff(time)
    dx = diff(x)
    nz = np.nonzero(np.abs(dx) < 1e-17)[0]
    tnz = time[nz] + 0.5*dt[nz]
    xnz = x[nz] + 0.5*dx[nz]
    plt.plot(tnz, xnz, 'o', markeredgecolor=color, markerfacecolor='none', markersize=sz)
    print 'got {0} small steps for {1}'.format(len(nz), name)


######################################################################

if len(sys.argv) != 2:
    print 'usage: python plot_results.py LOGFILENAME'
    sys.exit(0)

data = np.genfromtxt(sys.argv[1], )
assert(len(data.shape) == 2 and data.shape[1] == 4)

time = data[:,0]
cmd  = data[:,1]
ref  = data[:,2]
pos  = data[:,3]

nplots = 2

mid_index = data.shape[0]/2
start_index = mid_index - 200
end_index = mid_index + 200

dt = diff(time)
dcmd = diff(cmd)
dref = diff(ref)
dpos = diff(pos)

mean_dt = dt.mean()

print 'read {0} samples from {1}'.format(data.shape[0], sys.argv[1])
print 'mean dt is {0}'.format(mean_dt)

estimate_correlation('cmd', cmd, 'ref', ref, start_index, end_index, mean_dt)
estimate_correlation('cmd', cmd, 'pos', pos, start_index, end_index, mean_dt)

plt.subplot(nplots,1,1)
plt.plot(time[1:], dt)
plt.xlabel('Time (s)')
plt.ylabel('dt (s)')

plt.subplot(nplots,1,2)
plt.plot(time, cmd, 'g', label='cmd')
plt.plot(time, ref, 'r', label='ref')
plt.plot(time, pos, 'b', label='pos')
plt.xlabel('Time (s)')
plt.ylabel('Angle (rad)')

check_skip('cmd', time, cmd, 'g', 3)
check_skip('ref', time, ref, 'r', 6)
check_skip('pos', time, pos, 'b', 12)

if nplots == 3:
    plt.subplot(nplots,1,3)
    plt.plot(time[1:], 'g', dcmd / dt, label='cmd')
    plt.plot(time[1:], 'r', dref / dt, label='ref')
    plt.plot(time[1:], 'b', dpos / dt, label='pos')
    plt.xlabel('Time (s)')
    plt.ylabel('Angular rate (rad/s)')

plt.legend()
plt.show()
