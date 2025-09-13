"""Tests of the Python bindings for Hamlib
"""
import pytest
import sys

def pytest_addoption(parser):
    # using long options only because short options conflict with pytest's
    if sys.argv[1].endswith("amp.py"):
        parser.addoption('--model', type=int, default=1,
                        metavar='ID', help='select amplifier model number')
        parser.addoption('--amp-file', default=None,
                        metavar='DEVICE', help='set device of the amplifier to operate on')
    elif sys.argv[1].endswith("rig.py"):
        parser.addoption('--model', type=int, default=1,
                        metavar='ID', help='select radio model number')
        parser.addoption('--rig-file', default=None,
                        metavar='DEVICE', help='set device of the radio to operate on')
    elif sys.argv[1].endswith("rot.py"):
        parser.addoption('--model', type=int, default=1,
                        metavar='ID', help='select rotator model number')
        parser.addoption('--rot-file', default=None,
                        metavar='DEVICE', help='set device of the rotator to operate on')
    else:
        # fallbacks for invocations through a Makefile
        parser.addoption('--model', type=int, default=1)
        parser.addoption('--amp-file', default=None)
        parser.addoption('--rig-file', default=None)
        parser.addoption('--rot-file', default=None)
    parser.addoption('--serial-speed', type=int, default=0,
                     metavar='BAUD', help='set serial speed of the serial port')
    parser.addoption('--hamlib-verbose', action='count', default=0,
                     help='set verbose mode, cumulative')

@pytest.fixture
def model(request):
    return request.config.getoption("--model")

@pytest.fixture
def amp_file(request):
    return request.config.getoption("--amp-file")

@pytest.fixture
def rig_file(request):
    return request.config.getoption("--rig-file")

@pytest.fixture
def rot_file(request):
    return request.config.getoption("--rot-file")

@pytest.fixture
def serial_speed(request):
    return request.config.getoption("--serial-speed")

@pytest.fixture
def hamlib_verbose(request):
    return request.config.getoption("--hamlib-verbose")
