
import math
import scipy.integrate as intg
from matplotlib import pyplot as plt
import numpy as np

# mean and std dev of the distribution
u = 2
d = 0.5

# number of standard deviations higher than mean expected the cache size is
T = 0

class Dist:
    def __init__(self, A, C, k):
        self.A = A
        self.C = C
        self.k = k

    def f(self, x,i):
        return math.exp(-((x-i*u/self.k)**2)/(2*i*(d**2)/self.k))/(d*math.sqrt(2*math.pi*i/self.k))

    def W(self,x):
        return sum([math.comb(self.A,i)*((self.k/self.C)**i)*((1-self.k/self.C)**(self.A-i))*self.f(x,i) for i in range(1, 1+self.A)])

    def CS(self):
        return (u+T*d)*self.A/self.C

    def Var(self):
        return intg.quad(lambda x: self.W(x)*((x-self.CS())**2), 0, 8*d*u*self.A/self.C)[0]

# We want to compute the VARIANCE - i.e. a low variance means we are close to estimated cache capacity, high means we're off.
# Keep A,C fixed. k is what we want to vary.

max_A = 15
ks = range(1,6)
plt.plot(ks, [Dist(max_A,5,k).Var() for k in ks], 'r')
plt.show()
