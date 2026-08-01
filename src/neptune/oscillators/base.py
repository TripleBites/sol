from abc import ABC, abstractmethod

"""
This oscillator class was taken from https://python.plainenglish.io/making-a-synth-with-python-oscillators-2cb8e68e9c3b.
This tutorial at the link above had more details on how this works.
"""

class Oscillator(ABC):
    def __init__(self, freq=440, phase=0, amp=1, \
                 sample_rate=44_100, wave_range=(-1, 1)):
        self._base_freq = freq
        self._base_amp = amp
        self._base_phase = phase
        self._sample_rate = sample_rate
        self._wave_range = wave_range
        
        # Properties that will be changed
        self._altered_freq = freq
        self._altered_amp = amp
        self._altered_phase = phase
        
    @property
    def init_base_freq(self):
        return self._base_freq
    
    @property
    def init_base_amp(self):
        return self._base_amp
    
    @property
    def init_base_phase(self):
        return self._base_phase
    
    @property
    def freq(self):
        return self._altered_freq
    
    @freq.setter
    def freq(self, value):
        self._altered_freq = value
        self._post_base_freq_set()
        
    @property
    def amp(self):
        return self._altered_amp
    
    @amp.setter
    def amp(self, value):
        self._altered_amp = value
        self._post_base_amp_set()
        
    @property
    def phase(self):
        return self._altered_phase
    
    @phase.setter
    def phase(self, value):
        self._altered_phase = value
        self._post_base_phase_set()
    
    def _post_base_freq_set(self):
        pass
    
    def _post_base_amp_set(self):
        pass
    
    def _post_base_phase_set(self):
        pass
    
    @abstractmethod
    def _initialize_osc(self):
        pass
    
    @staticmethod
    def squish_val(val, min_val=0, max_val=1):
        return (((val + 1) / 2 ) * (max_val - min_val)) + min_val

    @abstractmethod
    def __next__(self):
        return None
    
    def __iter__(self):
        self.freq = self._base_freq
        self.phase = self._base_phase
        self.amp = self._base_amp
        self._initialize_osc()
        return self