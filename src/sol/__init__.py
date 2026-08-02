"""Sol Engine - A C99 game engine with SDL3 + Vulkan, wrapped for Python.

Python modules live in the python/ subdirectory for organization.
They are re-exported here so existing imports (sol.ui_bindings, etc.)
continue to work unchanged.
"""
import sys
import os.path as _path

# ---------------------------------------------------------------------------
# Load modules from python/ subdirectory in dependency order.
# Register each in sys.modules BEFORE loading dependents.
# ---------------------------------------------------------------------------
_py = _path.join(_path.dirname(__file__), 'python')

# 1. bindings (no internal sol.* deps)
from sol.python import bindings
sys.modules['sol.bindings'] = bindings

# 2. sol (depends on bindings)
from sol.python import sol as _sol_mod
sys.modules['sol.sol'] = _sol_mod

# 3. ui_bindings (depends on bindings)
from sol.python import ui_bindings
sys.modules['sol.ui_bindings'] = ui_bindings

# 4. audio_bindings (depends on bindings)
from sol.python import audio_bindings
sys.modules['sol.audio_bindings'] = audio_bindings

# 5. input_bindings (depends on bindings)
from sol.python import input_bindings
sys.modules['sol.input_bindings'] = input_bindings

# 6. photon_bindings (depends on bindings)
from sol.python import photon_bindings
sys.modules['sol.photon_bindings'] = photon_bindings

# 7. tui_bindings (depends on bindings)
from sol.python import tui_bindings
sys.modules['sol.tui_bindings'] = tui_bindings

# Public API
from sol.python.sol import init, update, shutdown, get_size

__all__ = ["init", "update", "shutdown", "get_size"]
