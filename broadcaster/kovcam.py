from sonien.broadcaster import WebSocketServer, JSMPSocket, SourceFIFO
import was, sys


if __name__ == '__main__' :
    pid_was = was.start(8400, True)
    if pid_was != 0 :
        print("[INFO] New PID(WAS) is %d" % pid_was)
    
    JSMPSocket.setVideoSize(int(sys.argv[1]), int(sys.argv[2]))
    fifo_camera     = SourceFIFO(sys.argv[4], True)
    brodcaster      = WebSocketServer(8410, fifo_camera, JSMPSocket, '/tmp/kovcam.broadcaster.pid')
    pid_brodcaster  = brodcaster.start(True)
    if pid_brodcaster != 0 :
        print("[INFO] New PID(BRO) is %d" % pid_brodcaster)
    
    print("Broadcaster start")
