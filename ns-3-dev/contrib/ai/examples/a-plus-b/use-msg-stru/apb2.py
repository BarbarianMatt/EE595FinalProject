import ns3ai_apb_py_stru as py_binding
from ns3ai_utils import Experiment
import sys
import traceback
import uuid
import time
import subprocess

import gc
from multiprocessing import Process
import os

def run_experiment(loop_id):

    unique_id = str(uuid.uuid4())
    print(f"Starting loop {loop_id}, PID: {os.getpid()}")

    filepath = '/mnt/c/Users/BarbarianMatt/Code/Python/EE595ProjectAI/ns-3-dev'

    setting = { "num_env": 2, 
                "m_segmentName":f"My_Seg_{unique_id}",
                "m_cpp2pyMsgName":f"My_Cpp_to_Python_Msg_{unique_id}",
                "m_py2cppMsgName":f"My_Python_to_Cpp_Msg_{unique_id}",
                "m_lockableName":f"My_Lockable_{unique_id}"
                }

    exp = Experiment(
        "ns3ai_apb_msg_stru",
        filepath,
        py_binding,
        handleFinish=True,
        segName=setting['m_segmentName'],
        cpp2pyMsgName=setting['m_cpp2pyMsgName'],
        py2cppMsgName=setting['m_py2cppMsgName'],
        lockableName=setting['m_lockableName']
    )

    result = subprocess.run(['ls', '/dev/shm'], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

    # Print the output (list of shared memory segments)
    if result.returncode == 0:
        print("Shared memory segments:")
        print(result.stdout)
    else:
        print(f"Error: {result.stderr}")

    
    print("Starting experiment...", flush=True)
    msgInterface = exp.run(setting=setting, show_output=True)
    print("Experiment started.", flush=True)
    start_time = time.time()
    try:
        for i in range(1000):
            
            
            msgInterface.PyRecvBegin()


            if msgInterface.PyGetFinished() or time.time() - start_time > 20:
                break


            # print(f"Received a: {msgInterface.GetCpp2PyStruct().a}, b: {msgInterface.GetCpp2PyStruct().b}")

            temp = msgInterface.GetCpp2PyStruct().a + msgInterface.GetCpp2PyStruct().b
            msgInterface.PyRecvEnd()

            msgInterface.PySendBegin()
            msgInterface.GetPy2CppStruct().c = temp
            msgInterface.PySendEnd()

            # print('', flush=True)
            # print(i, flush=True)

    except Exception as e:
        exc_type, exc_value, exc_traceback = sys.exc_info()
        print("Exception occurred: {}".format(e), flush=True)
        print("Traceback:", flush=True)
        traceback.print_tb(exc_traceback)
        exit(1)
    finally:
        print("Finally exiting...", flush=True)

    del exp
    del msgInterface
    gc.collect()
    # subprocess.run(['rm', '-f', f'/dev/shm/My_Seg_{unique_id}'])
    # time.sleep(5)


for j in range(1):
    process = Process(target=run_experiment, args=(j,))
    process.start()
    process.join()

    