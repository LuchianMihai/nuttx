; function definitions and body
(function_definition
    (storage_class_specifier)?
    type: (primitive_type)
    declarator: (function_declarator)
    body: (compound_statement) @function.body
)


(compound_statement
    (if_statement
    	"if" @keyword.if
    	condition: (parenthesized_expression) @condition.if
    	consequence: (compound_statement) @consequence.if
    	alternative: (else_clause)? @alternative.if
    ) @statement.if
)
