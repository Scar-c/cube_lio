#!/usr/bin/env python3
"""Minimal Eq. (16) / actual bilinear-residual derivative acceptance gate.

No geometry, seams, noise, IDW, GT or tuning. Cubic intensity has a quadratic,
positive derivative-of-Gaussian response; its magnitude is the IGM. The same
discrete Gaussian derivative used by the P4 Frame is reproduced here.
Exit 2 means the declared exact-residual derivative gate is unsatisfied.
"""
import json
import math


def main():
    offsets = list(range(-3, 4))
    gaussian = [math.exp(-0.5 * k * k) for k in offsets]
    total = sum(gaussian)
    gaussian = [g / total for g in gaussian]
    derivative = [k * g for k, g in zip(offsets, gaussian)]
    moment = sum(k * d for k, d in zip(offsets, derivative))
    derivative = [d / moment for d in derivative]

    def igm(pixel):
        gx = sum(d * (pixel + k + 16) ** 3
                 for k, d in zip(offsets, derivative))
        return abs(gx)  # Gy = 0, Gx > 0 in this region

    def sample(coordinate):
        pixel = math.floor(coordinate)
        alpha = coordinate - pixel
        return (1 - alpha) * igm(pixel) + alpha * igm(pixel + 1)

    coordinate = 20.37
    paper_gradient = (sample(coordinate + 1) - sample(coordinate - 1)) / 2
    local_derivative = igm(21) - igm(20)
    probes = []
    for step in (1e-4, 1e-5, 1e-6, 1e-7):
        actual = (sample(coordinate + step) - sample(coordinate - step)) / (2 * step)
        error = abs(paper_gradient - actual)
        probes.append({'step': step, 'residual_fd': actual, 'abs_error': error,
                       'accepted': error <= 1e-4 + 1e-4 * abs(actual)})
    result = {
        'status': 'PASS' if all(p['accepted'] for p in probes) else 'FAIL',
        'intensity': 'I(j)=(j+16)^3', 'igm': 'M(j)=3(j+16)^2+C, Gx positive, Gy zero',
        'coordinate': coordinate, 'paper_eq16': paper_gradient,
        'exact_bilinear_derivative': local_derivative,
        'symbolic_difference': '6*alpha-3 = -0.78 at alpha=0.37',
        'h_probes': probes,
    }
    print(json.dumps(result, indent=2))
    return 0 if result['status'] == 'PASS' else 2


if __name__ == '__main__':
    raise SystemExit(main())
