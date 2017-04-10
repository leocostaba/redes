import matplotlib.pyplot as plt

y = []
with open("teste") as f:
    for l in f:
        v = float(l)
        y.append(v)

y = y[:620]

x = range(0, len(y))
plt.plot(x, y)
plt.savefig("teste.png")
