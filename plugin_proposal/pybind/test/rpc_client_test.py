#!/usr/bin/env python3.12
from pathlib import Path
import sys
import time

print('not working yet')
sys.exit(-1)

sys.path.append((Path.home() / 'projects/up-client-zenoh-cpp/plugin_proposal/build/lib').as_posix())
from pyPluginApi import *

dll = Path.home() / 'projects/up-client-zenoh-cpp/plugin_proposal/build/lib/libapi_implementation.so'
plugin = PluginApi(dll.as_posix())
session = Session(plugin, "start_doc", "ses")

for i in range(10):
    f = queryCall(session, "demo/rpc/action1", Message(f'pay{i}', f'attr{i}'), "rpcClnt")
    result = f.get()
    print(result)
    time.sleep(100)

# trace_name = ses
# >>> pub = pyPluginApi.Publisher(s, "upl/p1", "p1")
# >>> quit()
