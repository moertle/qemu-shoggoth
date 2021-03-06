#/*
# * Rapid Analysis QEMU System Emulator
# *
# * Copyright (c) 2020 Cromulence LLC
# *
# * Distribution Statement A
# *
# * Approved for Public Release, Distribution Unlimited
# *
# * Authors:
# *  Joseph Walker
# *
# * This work is licensed under the terms of the GNU GPL, version 2 or later.
# * See the COPYING file in the top-level directory.
# * 
# * The creation of this code was funded by the US Government.
# */

import struct
import binascii
from abc import ABCMeta, abstractmethod
from enum import IntEnum

try:
    from _PyQemu import *
except ImportError:
    raise EnvironmentError("QEMU plugins must be run from within a QEMU instance.")

class Register(object):

    def __init__(self, cid, name):
        self.cpu_id = cid
        self.name = name
        self.size = get_cpu_register(self.cpu_id, self.name)[1]

    def getName(self):
        return self.name

    def __call__(self, arg):
        snd = []
        for x in range(self.size):
            tmp = arg & 0xff
            snd.append(tmp)
            arg = arg >> 8
        value = bytearray(snd)
        set_cpu_register(self.cpu_id, self.name, value)
        return self

    def __int__(self):
        value, _ = get_cpu_register(self.cpu_id, self.name)
        return int(binascii.hexlify(value), 16)

    def __iter__(self):
        return iter(get_cpu_register(self.cpu_id, self.name)[0])

    #def __float__(self):
    #    value = get_cpu_register(self.cpu_id, self.name)[0]
    #    return float(self.value)
    
    def __str__(self):
        return self.name

class CPU(object):
    def __init__(self, cpu_id = None):
        self.cpu_id = cpu_id if cpu_id is not None else get_current_cpu()

    def __getattr__(self, name):
        return Register(self.cpu_id, name.upper())
    
    def setVirtualMemory(self, address, data):
        VirtualMemory(self.cpu_id, address, len(data))(data)
    
    def getVirtualMemory(self, address, size):
        return bytearray(VirtualMemory(self.cpu_id, address, size))
    
    def getVirtualMemoryObj(self, address, size):
        return VirtualMemory(self.cpu_id, address, size)

    @staticmethod
    def setPhysicalMemory(address, data):
        PhysicalMemory(address, len(data))(data)

    @staticmethod
    def getPhysicalMemory(address, size):
        return bytearray(PhysicalMemory(address, size))

    @staticmethod
    def getPhysicalMemoryObj(address, size):
        return PhysicalMemory(address, size)

    @staticmethod
    def getRegisterNames():
        return get_register_names()

class MemoryBase(object):
    __metaclass__ = ABCMeta

    def __init__(self, address, size):
        self.address = address
        self.size = size
    
    def getAddress(self):
        return self.address
    
    def getSize(self):
        return self.size

    @abstractmethod
    def __iter__(self):
        raise NotImplementedError("MemoryBase does not implement __iter__")

    @abstractmethod
    def __call__(self, arg):
        raise NotImplementedError("MemoryBase does not implement __call__")

class VirtualMemory(MemoryBase):
    def __init__(self, cpu, address, size):
        super(VirtualMemory, self).__init__(address, size)
        self.cpu = cpu

    def __iter__(self):
        return iter(get_virtual_memory(self.cpu, self.address, self.size))

    def __call__(self, arg):
        if len(arg) <= self.size:
            set_virtual_memory(self.cpu, self.address, arg)
        else:
            raise Exception()

class PhysicalMemory(MemoryBase):
    def __init__(self, address, size):
        super(PhysicalMemory, self).__init__(address, size)

    def __iter__(self):
        return iter(get_physical_memory(self.address, self.size))

    def __call__(self, arg):
        if len(arg) <= self.size:
            set_physical_memory(self.address, arg)
        else:
            raise Exception()

class RapidAnalysis(object):
    @staticmethod
    def addJob(queue, jobmsg):
        ra_add_job(queue, bytes(jobmsg))

class WorkItem(object):
    
    def __init__(self):
        self.buffer = ''
        self.item_list = []

    def __delete__(self, instance):
        del self.buffer
        del self.item_list

class ResultItem(object):
    
    def __init__(self):
        self.buffer = ''


class QObjectTypes(IntEnum):
    QTYPE_QNULL = 1
    QTYPE_QNUM = 2
    QTYPE_QSTRING = 3
    QTYPE_QDICT = 4
    QTYPE_QLIST = 5
    QTYPE_QBOOL = 6


class _QObjectBase(object):

    def __init__(self, o_type):
        self.o_type = int(o_type)


class QDict(_QObjectBase):

    def __init__(self):
        super(QDict, self).__init__(QObjectTypes.QTYPE_QDICT)
        self.value = {}

    def __len__(self):
        return len(self.value)

    def __getitem__(self, idx):
        return self.value[idx]

    def __setitem__(self, idx, value):
        self.value[idx] = value

    def __iter__(self):
        return iter(self.value)

    def __str__(self):
        return str(self.value) 

    def __repr__(self):
        return str(self)

    def __bool__(self):
        return bool(self.value)

    def qdict_put(self, key, val):
        self.value[str(key)] = val

    def qdict_put_int(self, key, num):
        val = QNum(num)
        self.qdict_put(key, val)

    def qdict_put_str(self, key, string):
        val = QString(string)
        self.qdict_put(key, val)

    def qdict_put_null(self, key):
        val = QNull()
        self.qdict_put(key, val)

    def qdict_put_bool(self, key, b):
        val = QBool(b)
        self.qdict_put(key, val)


class QList(_QObjectBase):

    def __init__(self):
        super(QList, self).__init__(QObjectTypes.QTYPE_QLIST)
        self.value = []     

    def __len__(self):
        return len(self.value)

    def __getitem__(self, idx):
        return self.value[idx]

    def __setitem__(self, idx, value):
        self.value[idx] = value

    def __iter__(self):
        return iter(self.value)

    def __str__(self):
        return str(self.value) 

    def __repr__(self):
        return str(self)        

    def __bool__(self):
        return bool(self.value)

    def qlist_append(self, val):
        self.value.append(val)

    def qlist_append_bool(self, val):
        v = QBool(val)
        self.qlist_append(v)

    def qlist_append_int(self, val):
        v = QNum(val)
        self.qlist_append(v)

    def qlist_append_null(self):
        v = QNull()
        self.qlist_append(v)

    def qlist_append_str(self, val):
        v = QString(val)
        self.qlist_append(v)          


class QNum(_QObjectBase):
    
    def __init__(self, value):
        super(QNum, self).__init__(QObjectTypes.QTYPE_QNUM)       
        self.value = value 

    def __str__(self):
        return str(self.value)

    def __repr__(self):
        return str(self)        

    def __int__(self):
        return int(self.value)

    def __bool__(self):
        return bool(self.value)    


class QString(_QObjectBase):

    def __init__(self, value):
        super(QString, self).__init__(QObjectTypes.QTYPE_QSTRING)
        self.value = value

    def __str__(self):
        return str(self.value)

    def __repr__(self):
        return str(self)        

    def __bool__(self):
        return bool(self.value)


class QBool(_QObjectBase):

    def __init__(self, value):
        super(QBool, self).__init__(QObjectTypes.QTYPE_QBOOL)
        self.value = value

    def __str__(self):
        return str(self.value)

    def __repr__(self):
        return str(self)        

    def __bool__(self):
        return bool(self.value)        


class QNull(_QObjectBase):

    def __init__(self):
        super(QNull, self).__init__(QObjectTypes.QTYPE_QNULL)  
        self.value = None

    def __str__(self):
        return str(self.value)

    def __repr__(self):
        return str(self)

    def __bool__(self):
        return bool(self.value)

