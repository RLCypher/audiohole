"""Reproduce algebra/numerics; eigenvalues describe only the declared ideal model."""
import json
import math
import platform

result = {"kind": "synthetic arithmetic / linear-model analysis", "python": platform.python_version(),
          "period_frames": 128, "rate_hz": 48000, "gains": [], "rate_quantization": []}
try:
    import numpy as np
    result["numpy_version"] = np.__version__
except ImportError:
    np = None
    result["eigenvalue_limitation"] = "NumPy unavailable; eigenvalues not calculated"

period, sample_rate = 128, 48000
for bandwidth in (0.128, 0.016, 0.001):
    w = 2 * math.pi * bandwidth * period / sample_rate
    a, u, v = 1 - math.exp(-20*w), 1.5*w/period, w/1.5
    row = {"bandwidth": bandwidth, "omega": w, "w0": a, "w1": u, "w2": v,
           "first_correction_for_one_frame": 1-a*a*u*(1+v)}
    if np is not None:
        rem = 1-a
        z1 = np.array([a*u, rem, 0, 0])
        z2 = np.array([a*a*u, a*rem, rem, 0])
        z3 = np.array([0, 0, 0, 1]) + v*z2
        e = np.array([1, 0, 0, 0]) - period*(z2+z3)
        matrix = np.stack([e, z1, z2, z3])
        eig = np.linalg.eigvals(matrix)
        row["transition_matrix"] = matrix.tolist()
        row["poles"] = [{"real": x.real, "imag": x.imag, "modulus": abs(x)} for x in eig]
        assert max(abs(x) for x in eig) < 1
    result["gains"].append(row)

for ppm in (-100, -1, 1, 100):
    correction = 1+ppm*1e-6
    effective_input = int(sample_rate/correction)
    result["rate_quantization"].append({"command_ppm": ppm, "effective_input": effective_input,
        "output_over_input": sample_rate/effective_input,
        "effective_ppm": (sample_rate/effective_input-1)*1e6})
result["ring_wrap_hours_stereo_float_48k"] = 2**32/(48000*2*4)/3600
result["uncorrected_100ppm_128frame_margin_seconds"] = 128/(48000*100e-6)
print(json.dumps(result, indent=2))
