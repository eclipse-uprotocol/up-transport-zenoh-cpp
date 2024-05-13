#!/usr/bin/env python3.12
from pathlib import Path
import sys
import time

sys.path.append((Path.home() / 'projects/up-client-zenoh-cpp/plugin_proposal/build/lib').as_posix())
from pyPluginApi import *

dll = Path.home() / 'projects/up-client-zenoh-cpp/plugin_proposal/build/lib/libapi_implementation.so'
plugin = PluginApi(dll.as_posix())
session = Session(plugin, "start_doc", "ses")
p1 = Publisher(session, "upl/p1", "p1")

for i in range(10):
    print(i)
    p1(f'hello{i}', f'world{i}')
    time.sleep(1)
# trace_name = ses
# >>> pub = pyPluginApi.Publisher(s, "upl/p1", "p1")
# >>> quit()
