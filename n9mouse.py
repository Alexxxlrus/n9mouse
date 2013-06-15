
from PySide.QtGui import QApplication, QWidget

import sys
import socket

class Widget(QWidget):
    def __init__(self):
        QWidget.__init__(self)
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    def _send(self, msg, pos):
        self.sock.sendto('%s,%d,%d' % (msg, pos.x(), pos.y()),
                ('192.168.2.14', 9977))

    mousePressEvent = lambda self, e: self._send('press', e.pos())
    mouseMoveEvent = lambda self, e: self._send('move', e.pos())
    mouseReleaseEvent = lambda self, e: self._send('release', e.pos())

app = QApplication(sys.argv)

w = Widget()
w.showFullScreen()

sys.exit(app.exec_())

