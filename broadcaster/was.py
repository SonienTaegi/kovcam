'''
Created on 2015. 2. 1.

@author: SonienTaegi
'''
from http.server import HTTPServer, BaseHTTPRequestHandler
import os
from posix import fork, setsid, chdir
import re
import sys


class StaticProvider(BaseHTTPRequestHandler):
    site_root = os.getcwd() + "/contents" 
    
    @staticmethod
    def setSiteRoot(pathSiteRoot):
        StaticProvider.site_root = pathSiteRoot
    
    def log_message(self, format, *args):
        pass
        
    def do_GET(self): 
        path_info = self.path
        if path_info == "/" :
            path_info = "/index.html"
        path_info = StaticProvider.site_root + path_info
        
        try :
            file = open(file = path_info, mode = 'rb')
        except IOError :
            self.send_error(404)
            return False
    
        contentType = 'text/plain'
        m = re.compile('\.[a-zA-Z0-9]+$').search(path_info)
        if m :
            ext = m.group(0) 
            if ext == ".js" :
                contentType = 'text/javascript'
            elif ext == ".html" or ext == ".htm" :
                contentType = 'text/html'
            elif ext == ".css" :
                contentType = 'text/css'
            elif ext == ".xml" :
                contentType = 'text/xml'
        
        self.send_response(200)
        self.send_header('Content-type', contentType + '; charset=utf-8')
        self.send_header("Content-Length", str(os.path.getsize(path_info)))
        self.end_headers()
        
        isEOF = False
        while not isEOF :
            try :
                data = file.read()
                if data == b'' :
                    isEOF = True
                else :
                    self.wfile.write(data)
            except IOError :
                isEOF = True

def start(nPort = 80, isDaemon = False, pid_file_path = '/tmp/kovcam.was.pid') :
    if isDaemon :
        pid = fork()
        if pid :
            pid_file = open(pid_file_path, mode='wt')
            pid_file.write("%d" % pid)
            pid_file.close() 
            return pid
        else :
            os.setsid()
            os.chdir('/')
            os.umask(0)
        
    httpd = HTTPServer(('', nPort), StaticProvider)    
    httpd.serve_forever()
    return 0

if __name__ == '__main__' :
    if len(sys.argv) > 1 :
        StaticProvider.setSiteRoot(sys.argv[1])
        
    pid = start(nPort = 8420, isDaemon = False)
    if pid :
        print("PID : %d" % pid)
        sys.exit(0)
    
    print("Daemon is terminated")
    