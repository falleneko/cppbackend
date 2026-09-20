import argparse
import random
import shlex
import signal
import subprocess
import time
from pathlib import Path

RANDOM_LIMIT = 1000
SEED = 123456789
random.seed(SEED)

AMMUNITION = [
    'localhost:8080/api/v1/maps/map1',
    'localhost:8080/api/v1/maps',
]

SHOOT_COUNT = 100
COOLDOWN = 0.1

SCRIPT_DIR = Path(__file__).resolve().parent
PERF_DATA = SCRIPT_DIR / 'perf.data'
GRAPH = SCRIPT_DIR / 'graph.svg'
FLAMEGRAPH_DIR = SCRIPT_DIR / 'FlameGraph'


def start_server():
    parser = argparse.ArgumentParser()
    parser.add_argument('server', type=str)
    return parser.parse_args().server


def run(command, output=None, input_=None):
    return subprocess.Popen(
        shlex.split(command),
        stdin=input_,
        stdout=output,
        stderr=subprocess.DEVNULL,
    )


def stop(process, wait=False):
    if process.poll() is not None:
        return

    if wait:
        process.wait()
    else:
        process.terminate()
        process.wait()


def shoot(ammo):
    hit = run('curl ' + ammo, output=subprocess.DEVNULL)
    time.sleep(COOLDOWN)
    stop(hit, wait=True)


def make_shots():
    for _ in range(SHOOT_COUNT):
        ammo_number = random.randrange(RANDOM_LIMIT) % len(AMMUNITION)
        shoot(AMMUNITION[ammo_number])
    print('Shooting complete')


def record(server):
    command = f'perf record -g -o {shlex.quote(str(PERF_DATA))} -p {server.pid}'
    return run(command)


def stop_recording(perf):
    if perf.poll() is None:
        # perf writes the trailer and closes perf.data when interrupted.
        perf.send_signal(signal.SIGINT)
    return_code = perf.wait()
    if return_code:
        raise subprocess.CalledProcessError(return_code, perf.args)


def make_flamegraph():
    with GRAPH.open('wb') as graph:
        perf_script = subprocess.Popen(
            ['perf', 'script', '-i', str(PERF_DATA)],
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
        )
        collapse = subprocess.Popen(
            [str(FLAMEGRAPH_DIR / 'stackcollapse-perf.pl')],
            stdin=perf_script.stdout,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
        )
        perf_script.stdout.close()
        flamegraph = subprocess.Popen(
            [str(FLAMEGRAPH_DIR / 'flamegraph.pl')],
            stdin=collapse.stdout,
            stdout=graph,
            stderr=subprocess.DEVNULL,
        )
        collapse.stdout.close()

        return_codes = (
            flamegraph.wait(),
            collapse.wait(),
            perf_script.wait(),
        )

    commands = (flamegraph.args, collapse.args, perf_script.args)
    for return_code, command in zip(return_codes, commands):
        if return_code:
            raise subprocess.CalledProcessError(return_code, command)


server = run(start_server())
perf = None
try:
    perf = record(server)
    time.sleep(1)
    make_shots()
finally:
    try:
        if perf is not None:
            stop_recording(perf)
    finally:
        stop(server)

make_flamegraph()
print('Job done')
