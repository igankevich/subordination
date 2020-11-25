import sys
import sbn


class Child2(sbn.Kernel):

    def act(self):
        print('Python: Py_kernel.act', file=sys.stderr, flush=True)
        self.data = "Child2Data"
        sbn.commit(kernel=self)


class Child(sbn.Kernel):

    def act(self):
        print('Python: Py_kernel.act', file=sys.stderr, flush=True)
        self.data = "ChildData"
        sbn.upstream(parent=self, child=Child2())

    def react(self, child2):
        print('Python: Child.react', file=sys.stderr, flush=True)
        print('Python: Child2.data =>', child2.data, file=sys.stderr, flush=True)
        self.data += "_" + child2.data
        sbn.commit(kernel=self)


class Main(sbn.Kernel):

    def act(self):
        print('Python: Main.act', file=sys.stderr, flush=True)
        sbn.upstream(parent=self, child=Child())

    def react(self, child):
        print('Python: Main.react', file=sys.stderr, flush=True)
        print('Python: Child.data =>', child.data, file=sys.stderr, flush=True)
        sbn.commit(kernel=self)
