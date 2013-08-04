import struct

HOST = '/tmp/indexd.sock'

COMMAND_ADD_DOC, COMMAND_QUERY_TOP, COMMAND_FORCE_FLUSH, COMMAND_EXIT, COMMAND_QUERY_FILTER_RELEVANCE_ID, COMMAND_UPDATE_DOCID_TO_TIME, COMMAND_QUERY_BUZZOMETRE = map(lambda x: struct.pack('B', x), range(1,8))

QNODE_AND, QNODE_OR, QNODE_KW, QNODE_EXACT, QNODE_APPROX, QNODE_NOT = map(lambda x: struct.pack('B', x), range(1,7))

pack_docID = lambda docID: struct.pack('I', docID)

def pack_word(word):
    return struct.pack('B%ss' % len(word), len(word), word.encode('utf-8'))