import nodes
from parsers import parse

"""
This file contains a function to parse a query.
It will have to return a rootnode.
"""

BINARY_OPERATOR, UNARY_OPERATOR, MODIFIER, TERM = range(4)

# Contains a tuple : The first element is the node, the second is the priority of the operator (the bigger it is, the more the operator have a big priority)

OPERATORS = {
    'AND': (nodes.AndNode, 5, BINARY_OPERATOR),
    'OR': (nodes.OrNode, 10, BINARY_OPERATOR),
    '&&': (nodes.AndNode, 5, BINARY_OPERATOR),
    '||': (nodes.OrNode, 10, BINARY_OPERATOR),
    #' -': (nodes.NotNode, 20, UNARY_OPERATOR),
    'NOT': (nodes.NotNode, 20, UNARY_OPERATOR),
}
MODIFIERS = {
    '"': nodes.ExactNode,
    '`': nodes.ApproxNode,
    '[': nodes.ApproxNode,
    ']': nodes.ApproxNode,
}

def get_type(term):
    if term in OPERATORS:
        return OPERATORS[term][2]
    elif term in MODIFIERS:
        return MODIFIER
    return TERM


def parse_query(query):
    def append_operator(term):
        assert not(lastType in (BINARY_OPERATOR, UNARY_OPERATOR) and get_type(term) == BINARY_OPERATOR)

        if get_type(term) == UNARY_OPERATOR and lastType == TERM:
            operators.append('AND')
        
        while len(operators) > 0 and OPERATORS[term][1] < OPERATORS[operators[-1]][1]:
            if get_type(operators[-1]) == UNARY_OPERATOR:
                terms.append( OPERATORS[ operators.pop() ][0](terms.pop()) )
            else:
                assert get_type(operators[-1]) == BINARY_OPERATOR
                terms.append( OPERATORS[ operators.pop() ][0] ( terms.pop(), terms.pop() ) )
            
        operators.append(term)


    for r in list(OPERATORS.keys()) + list(MODIFIERS.keys()) + ['(',')']:
        query = query.replace(r, ' ' + r + ' ')
    query = query.split(' ')

    terms = []
    operators = []
    lastType = BINARY_OPERATOR

    parenthesis_level = 0
    parenthesis_start = -1

    modifier = None
    modifier_terms = []

    for pos, term in enumerate(query):
        if not term:
            continue

        # Parenthesis
        if term == '(':
            parenthesis_level += 1
            if parenthesis_level == 1:
                parenthesis_start = pos + 1
        elif term == ')':
            parenthesis_level -= 1
            if parenthesis_level == 0:
                if lastType == TERM:
                    append_operator('AND')
                terms.append( parse_query(' '.join(query[parenthesis_start:pos])) )
                lastType = TERM
            continue
        if parenthesis_level > 0:
            continue

        # Modifier
        if get_type(term) == MODIFIER:
            if modifier is None:
                modifier = MODIFIERS[term]
            else:
                assert MODIFIERS[term] == modifier

                if lastType == TERM:
                    append_operator('AND')

                terms.append(modifier(modifier_terms))
                lastType = TERM
                modifier = None
                modifier_terms = []
            continue
        if modifier is not None:
            term_list = parse(term)
            modifier_terms.extend(nodes.KwNode(i) for i in term_list)
            continue

        # Operator or terms

        if get_type(term) in (BINARY_OPERATOR, UNARY_OPERATOR):
            append_operator(term)

        else:
            term_list = tuple(parse(term))
            if len(term_list) == 0:
                continue
            elif len(term_list) == 1:
                terms.append(nodes.KwNode(term_list[0]))
            else:
                terms.append(nodes.ExactNode([nodes.KwNode(i) for i in term_list]))

            if lastType == TERM:
                append_operator('AND')
        
        lastType = get_type(term)

    assert len(terms) > 0

    while len(terms) > 1:
        if get_type(operators[-1]) == UNARY_OPERATOR:
            terms.append( OPERATORS[ operators.pop() ][0](terms.pop()) )
        else:
            assert get_type(operators[-1]) == BINARY_OPERATOR
            terms.append( OPERATORS[ operators.pop() ][0] ( terms.pop(), terms.pop() ) )

    return terms[0]
