import config
import struct

"""
This file contains the implementation of the nodes, that will be sent to the index daemon.
You have to construct the node tree, and then you just send it to the daemon by doing rootnode.send(socket)
"""

# ABSTRACT NODES

class QueryNode(object):
    pass

class DoubleNode(QueryNode):
    def __init__(self, left, right):
        assert isinstance(left, QueryNode) and isinstance(right, QueryNode)
        self.left, self.right = left, right

    def __repr__(self):
        return "%s(%s, %s)" % (self.__class__.__name__, self.left, self.right)

    def send(self, socket):
        socket.send(self.node_id)
        self.left.send(socket)
        self.right.send(socket)

class MultiNode(QueryNode):
    def __init__(self, nodelist):
        assert min(isinstance(i, QueryNode) for i in nodelist)
        assert len(nodelist) > 1
        self.nodelist = nodelist

    def __repr__(self):
        return "%s(%s)" % (self.__class__.__name__, self.nodelist)

    def send(self, socket):
        socket.send(self.node_id)
        socket.send(struct.pack('H', len(self.nodelist)))
        for node in self.nodelist:
            node.send(socket)

class UniNode(QueryNode):
    def __init__(self, node):
        assert isinstance(node, QueryNode)
        self.node = node

    def __repr__(self):
        return "%s(%s)" % (self.__class__.__name__, self.node)

    def send(self, socket):
        socket.send(self.node_id)
        self.node.send(socket)


# NODES

class KwNode(QueryNode):
    def __init__(self, word):
        assert '/' not in word
        assert len(word) < 50
        self.word = word

    def __repr__(self):
        return "%s(%s)" % (self.__class__.__name__, self.word)

    def send(self, socket):
        socket.send(config.QNODE_KW)
        socket.send(config.pack_word(self.word))

class OrNode(DoubleNode):
    node_id = config.QNODE_OR

class AndNode(DoubleNode):
    node_id = config.QNODE_AND

class NotNode(UniNode):
    node_id = config.QNODE_NOT

class ExactNode(MultiNode):
    node_id = config.QNODE_EXACT

class ApproxNode(MultiNode):
    node_id = config.QNODE_APPROX
