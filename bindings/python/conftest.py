"""Tests of the Python bindings for Hamlib
"""
import pytest

def pytest_addoption(parser):
    # using long options only because short options conflict with pytest's
    parser.addoption('--model', type=int, default=1,
                     metavar='ID', help='select radio model number')
    parser.addoption('--rig-file', default=None,
                     metavar='DEVICE', help='set device of the radio to operate on')
    parser.addoption('--serial-speed', type=int, default=0,
                     metavar='BAUD', help='set serial speed of the serial port')
    parser.addoption('--hamlib-verbose', action='count', default=0,
                     help='set verbose mode, cumulative')

@pytest.fixture
def model(request):
    return request.config.getoption("--model")

@pytest.fixture
def rig_file(request):
    return request.config.getoption("--rig-file")

@pytest.fixture
def serial_speed(request):
    return request.config.getoption("--serial-speed")

@pytest.fixture
def hamlib_verbose(request):
    return request.config.getoption("--hamlib-verbose")
