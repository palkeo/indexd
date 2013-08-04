This is a client implementation, to add documents and make queries.

You can use it like this : ::
    from indexclient import IndexClient
    
    c = IndexClient()
    
    top_20 = c.query_top("cat", 20)
    
    # top_20 is now a list of docID

