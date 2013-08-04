# -*- coding: utf-8 -*-

import socket
import config
import parser
import struct
import datetime, time

class IndexClient():

    def __init__(self):
        self.socket = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.socket.connect(config.HOST)

    def add_document(self, docID, wordList):
        self.socket.send(config.COMMAND_ADD_DOC)
        self.socket.send(config.pack_docID(docID))
        self.socket.send(struct.pack('I', len(wordList)))
        self.socket.send(struct.pack('I', sum(word.weight for word in wordList)))

        for word in wordList:
            self.socket.send(config.pack_word(word) + struct.pack('B', word.weight))

    def query_top(self, query, size, start=0, docID_start=0, docID_stop=0):
        self.socket.send(config.COMMAND_QUERY_TOP)
        parser.parse_query(query).send(self.socket)
        self.socket.send(config.pack_docID(docID_start))
        self.socket.send(config.pack_docID(docID_stop))
        self.socket.send(struct.pack('I', start))
        self.socket.send(struct.pack('I', size))
        self.query_total = struct.unpack('I', self.socket.recv(struct.calcsize('I')))[0]

        l = []
        while True:
            docID = struct.unpack('I', self.socket.recv(struct.calcsize('I')))[0]
            if docID == 0:
                break
            l.append(docID)
        return l

    def query_filter(self, query, min_relevance=0, docID_start=0, docID_stop=0):
        self.socket.send(config.COMMAND_QUERY_FILTER_RELEVANCE_ID)
        parser.parse_query(query).send(self.socket)
        self.socket.send(struct.pack('f', min_relevance / 100.))
        self.socket.send(config.pack_docID(docID_start))
        self.socket.send(config.pack_docID(docID_stop))

        while True:
            docID = struct.unpack('I', self.socket.recv(struct.calcsize('I')))[0]
            if docID == 0:
                break
            yield docID

    def force_flush(self):
        self.socket.send(config.COMMAND_FORCE_FLUSH)

    def exit(self):
        self.socket.send(config.COMMAND_EXIT)
