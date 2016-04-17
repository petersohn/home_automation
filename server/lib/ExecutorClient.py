#!/usr/bin/env python3

import pickle
import socket


class ExecutorClient:
    def __init__(self, address):
        self.address = address
        self.socket = socket.socket(
            family=socket.AF_UNIX, type=socket.SOCK_DGRAM)

    def send(self, object):
        serialized_data = pickle.dumps(object)
        self.socket.sendto(serialized_data, self.address)
