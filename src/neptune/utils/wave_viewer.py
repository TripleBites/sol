import sys
import pyqtgraph as pg
from pyqtgraph.Qt import QtWidgets, QtCore

class WaveViewer(QtWidgets.QMainWindow):
    def __init__(self, probes):
        super().__init__()
        self.probes = probes
        self.setWindowTitle("Synth Node Inspector")
        self.resize(800, 400)
        
        # Setup the graph
        self.graph = pg.PlotWidget()
        self.setCentralWidget(self.graph)
        self.graph.setYRange(-2.5, 2.5)
        self.graph.showGrid(x=True, y=True, alpha=0.3)
        self.graph.addLegend()
        
        # Create a line for each probe in our system
        self.lines = {}
        colors = [(255, 100, 100), (100, 255, 100), (100, 100, 255)]
        
        for i, probe in enumerate(self.probes):
            color = colors[i % len(colors)]
            self.lines[probe.name] = self.graph.plot(
                name=probe.name, 
                pen=pg.mkPen(color=color, width=2)
            )
            
        # Setup a timer to pull from the probe buffers at ~30 FPS
        self.timer = QtCore.QTimer()
        self.timer.timeout.connect(self.update_plot)
        self.timer.start(33)
        
    def update_plot(self):
        for probe in self.probes:
            # Cast the deque to a list to pass it to the graphing engine
            self.lines[probe.name].setData(list(probe.buffer))