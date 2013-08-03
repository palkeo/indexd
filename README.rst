======
indexd
======

What is that ?
==============

This is a *very* minimalist inverted index implementation.

It is used for one of my personnal project.

I have tested it by indexing the complete version of the
French Wikipedia, and it works like a charm.

It is a daemon that will start a server.
Once you connect to it, you can add new documents,
and execute queries among the indexed documents.

Adding documents
================

Check the add_document function in the client_thread.cpp file.

Each document is referenced by a number, called the docID.

Executing queries
=================

You have two functions you can call (in client_thread.cpp) :

- query_top, that will retrieve the docID of the N documents
  that have the best relevance for the entered query.

- query_filter_relevance_id, that will retrieve the docID of
  every document that have a relevance greater than a specified
  amount, for the specified query.

What are queries ?
==================

The main concept is that **queries are trees**.

For exemple, the query « spam AND egg » is stored as :
AndNode(KwNode("spam"), KwNode("egg"))

You can find the implementation of each node in query.cpp.

The KwNode will read a file containing the list of documents
containing a word, and return every entry.
So, in a query tree, every leaf is a KwNode (or a DeltaStructNode,
that reads the entries temporarily stored in memory when updating
an index file).

These enties are then processed and combined by the logical nodes :
OrNode, AndNode, ExactNode…

How is the inverted index stored ?
==================================

For every word, we store the list of the documents containing that
word in one file.

So, you can easily have a LOT of files.
On my setup, I have now more that 50 millions of files.

So, please use a filesystem able to handle the load and manage
a lot of small files. BTRFS seems to works fine.

To try to reduce a little the number of files in the directories,
there is one subdirectory for each first letter of the words.
