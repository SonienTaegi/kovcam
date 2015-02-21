'''
Created on 2015. 2. 5.

@author: SonienTaegi
'''
"""
    class Source
    Base class of Source object for WebSocketServer.
    
    Source is base class of sources of WebSocketServer; for example SourceFIFO.
    WebSocketServer will call each method in this sequence :
        open > start > stop
    Each abstract methods should be implemented properly for their own purposes.
    
    Below event handlers will be called under each conditions 
        onReadHandler(source, data, size) : Read data from source successfully
        onEOSHandler(source)              : Reading source is finished
        onBrokenHandler(source)           : Source is broken
        
    Thread will turn into waitting after being called start() by WebSocketServer if source is opened in blocking mode.
"""

from abc                    import ABCMeta, abstractmethod
import os
from posix                  import fork
import threading
from wsgiref.simple_server  import make_server

from ws4py.server.wsgirefserver import WebSocketWSGIRequestHandler, WSGIServer
from ws4py.server.wsgiutils import WebSocketWSGIApplication
from ws4py.websocket        import WebSocket


class Source(metaclass=ABCMeta):
    def __init__(self):
        self.onReadHandler      = None
        self.onBrokenHandler    = None
        self.onEOSHandler       = None 
        self._isValid           = False
    
    """
        open()
        
        This method will be called by WebSocketServer to open the source.
        Please implement the way to open source. 
    """
    @abstractmethod
    def open(self):
        pass
    
    """
        clase()
        
        This method will be called by WebSocketServer to close / terminate source.
        Please implement the way to close source.
    """
    @abstractmethod
    def close(self):
        pass
    
    """
        read()
        
        return (code, data)
            code > 0    : Size of data
            code = 0    : End Of Source
            code < 0    : Broken
        
        This method will be called by Source itself to read data from source.
        Please implement the way to read source. 
    """
    @abstractmethod
    def read(self):
        pass
    
    """
        broken()
        
        This method will be called by Source itself when source is broken.
        Please implement the way to handle this situation.
    """
    @abstractmethod
    def broken(self):
        pass
    
    """
        eos()
        
        This method will be called by Source itselt when it meets end of source / stream / file.
        Please implement the way to handle this situation.
    """
    @abstractmethod
    def eos(self):
        pass
    
    """
        start()
        
        This method will be called by WebSocketServer to start to read data from source.
        Thread will turn into waitting after being called start() by WebSocketServer if source is opened in blocking mode.
    """
    def start(self):        
        self.open()
        
        self._isValid = True
        while self._isValid :
            result = self.read()
            if result[0] > 0 :
                if self.onReadHandler :
                    self.onReadHandler(self, result[1], result[0])
                    
            elif result[0] == 0 :
                if not self.onEOSHandler :
                    self.onEOSHandler(self)
                self.eos()
            
            else :
                if not self.onBrokenHandler :
                    self.onBrokenHandler(self)
                self.broken()        
        
    """ 
        stop()
       
        This method is optional to set source in Invalid mode. This method doesn't guaranty to terminate Source stream.
    """
    def stop(self):
        self._isValid = False

"""
    class SourceFIFO(path of FIFO file, reconnect = False)
    FIFO implemented Source class for WebSocketServer.
    
    SourceFIFO provides data from FIFO. FIFO will be opened binary - blocking mode without buffering.
    SourceFIFO doesn't have end-of-file so eos() will never be called.
    If reconnect is True, SourceFIFO waits up until FIFO connects again when it is broken.
    So calling stop() needs for closing FIFO and terminate source.
"""    
class SourceFIFO(Source):
    def __init__(self, strFifoPath, reconnect = False):
        Source.__init__(self)
        self._reconnect = reconnect
        self._strFifoPath = strFifoPath
        self._file = None
    
    def open(self):
        self._file = open(self._strFifoPath, "br", buffering=0)
        
    def stop(self):
        # Make Source into invalid mode
        Source.stop(self)
        
        # Force to close FIFO file
        self._file.close()
        
    def read(self):
        data = self._file.read(1024 * 1024)
        size = len(data)
        if size :
            return (size, data)
        else :
            return (-1, None)
    
    def close(self):
        self._file = None
        self._strFifoPath = None
        
    def eos(self):
        self.stop()
    
    def broken(self):
        if self._reconnect :
            print("[FAIL] FIFO is broken. Wait for reconnect")
            self._file.close()
            self.open()
            print("[FAIL] FIFO connected again")
        else :
            self._reader.stop()

class JSMPSocket(WebSocket):
    width   = 640
    height  = 480

    @staticmethod  
    def setVideoSize(width, height):
        JSMPSocket.width = width
        JSMPSocket.height = height
         
    def __init__(self, sock, protocols=None, extensions=None, environ=None, heartbeat_freq=None) :
        WebSocket.__init__(self, sock, protocols, extensions, environ, heartbeat_freq)
        self._payload = None
        self._binary  = False
        self._lock    = threading.Lock()
        self._sem     = threading.Semaphore(0)
        self._data    = None
        self._isValid = True
        threading.Thread(target=self.run).start()

    def run(self) :
        while self._isValid :
            self._sem.acquire()
            super().send(self._payload, self._binary)
            self._payload = None    

    def opened(self):
        WebSocket.opened(self)
        
        start_code = bytearray(b'jsmp')
        start_code.append(int(self.width / 256))
        start_code.append(self.width % 256)
        start_code.append(int(self.height / 256))
        start_code.append(self.height % 256)
        super().send(bytearray(start_code), True)

    def send(self, payload, binary=False) :
        with self._lock :
             if self._payload : return
             self._payload = payload
             self._binary  = binary
             self._sem.release()
        
class WebSocketServer:
    def __init__(self, nPort, source, handler_cls=WebSocket, pid_file_path='/tmp/sonien.broadcaster.pid'):
        self._nPort                  = nPort
        self._handler_cls            = handler_cls
        self._pid_file_path          = pid_file_path
        self._source                 = source
        self._source.onReadHandler   = self.onRead
        self._source.onEOSHandler    = self.onEOS
        self._source.onBrokenHandler = self.onBroken

    def start(self, isDaemon = False):
        if isDaemon :
            pid = fork() 
            if pid :
                pid_file = open(self._pid_file_path, mode='wt')
                pid_file.write("%d" % pid)
                pid_file.close()
                return pid
            else : 
                os.setsid()
                os.chdir('/')
                os.umask(0)
        
        self._server = make_server('', self._nPort, server_class=WSGIServer,
                                    handler_class=WebSocketWSGIRequestHandler,
                                    app=WebSocketWSGIApplication(handler_cls=self._handler_cls))
        self._server.initialize_websockets_manager()
        threading.Thread(target=self._server.serve_forever).start()
        self._source.start()
        return 0
        
    def onRead(self, source, data, size):
        # print("[INFO] Read %d bytes" % size)
        # self._server.manager.broadcast(data, True)
        with self._server.manager.lock :
            for ws in self._server.manager :
                ws.send(data, True)            
    
    def onEOS(self, source):
        print("[INFO] EOS")
    
    def onBroken(self, source):
        print("[FAIL] BROKEN")
    
if __name__ == '__main__' :
    brodcaster      = WebSocketServer(8410, SourceFIFO('/tmp/kovcam.fifo', True), JSMPSocket, '/tmp/kovcam.broadcaster.pid')
    pid_brodcaster  = brodcaster.start(False)
    
    
    
