import sbn
import sys

class Child2(sbn.kernel):

    def act(self):
        print('>>>> Python: Child2.act')
        self.data = "Child2Data"
        sbn.kernel_commit(kernel=self)


class Child(sbn.kernel):

    def act(self):
        print('>>>> Python: Child.act')
        self.data = "ChildData"
        sbn.kernel_upstream(parent=self, child=Child2())

    def react(self, child2):
        print('>>>> Python: Child.react')
        print('>>>> Python: child2.data =>', child2.data)
        self.data += "_" + child2.data
        sbn.kernel_commit(kernel=self)


class Main(sbn.kernel):
    # def __init__(self):
    #     super(Main, self).__init__()
    #     print('Create!')

    def act(self):
        print('>>>> Python: Main.act')
        print(sys.argv)
        sbn.kernel_upstream(parent=self, child=Child())

    def react(self, child):
        print('>>>> Python: Main.react')
        print('>>>> Python: child.data =>', child.data)
        sbn.kernel_commit(kernel=self)
