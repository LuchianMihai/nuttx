"""

"""
from pathlib import Path
import importlib.resources
import sys
from abc import ABC, abstractmethod
from typing import Generator
from argparse import Namespace

from tree_sitter import Language, Parser, Tree, Node, Query, Point
from tree_sitter_language_pack import get_language, get_parser


class Checker(ABC):
    """
    Base class for analyzing and processing syntax trees.
    This class is needed to avoid a single monolitic class checking
    all filetypes such as c/cpp headers/sources

    :param tree: The Tree-sitter syntax tree to analyze.
    :type tree: Tree
    :param parser: The Tree-sitter parser instance.
    :type parser: Parser
    :param lang: The Tree-sitter language instance.
    :type lang: Language
    :param scm: File name holding Tree-sitter queries
    "type scm: String
    """
    def __init__(self, tree: Tree, parser: Parser, lang: Language, scm: str):
        self.tree: Tree = tree
        self.parser: Parser = parser
        self.lang: Language = lang

        try:
            with importlib.resources.open_text("data.queries", scm) as f:
                queries: Query = self.lang.query(f.read())
        except FileNotFoundError as e:
            print(f"{e}")
            sys.exit(1)

        self.captures = queries.captures(self.tree.root_node)


    def gen_dot_graph(self, graph_path: Path):
        """
        Generates a DOT representation of the syntax tree and saves it to a file.

        :param graph_path: Path to the output DOT file.
        :type graph_path: Path
        :raises FileNotFoundError: If the specified file path is invalid.
        """
        try:
            with open(graph_path, 'wb') as gfd:
                self.tree.print_dot_graph(gfd)
        except FileNotFoundError as e:
            print(f"{e}")
            sys.exit(1)

    def walk_tree(self, node: Node | None = None) -> Generator[Node, None, None]:
        """
        Helper function to traverse the syntax tree in a depth-first manner, yielding each node.
        Traversing the tree is parser/language agnostic, 
        so this method should be part of the base class 
        
        :param node: The starting node for traversal. If None, starts from the root.
        :type node: Node | None
        :yield: Nodes in the syntax tree.
        :rtype: Generator[Node, None, None]
        """
        if node is None:
            cursor = self.tree.walk()
        else:
            cursor = node.walk()

        visited_children = False

        while True:
            if not visited_children:
                yield cursor.node
                if not cursor.goto_first_child():
                    visited_children = True
            elif cursor.goto_next_sibling():
                visited_children = False
            elif not cursor.goto_parent():
                break

    def info(self, point: Point, text: str) -> None:
        print(
            f"INFO[{point.row + 1}:{point.column}] "
            f"{text}")

    def warning(self, point: Point, text: str) -> None:
        print(
            f"WARNING[{point.row + 1}:{point.column}] "
            f"{text}")

    def error(self, point: Point, text: str) -> None:
        print(
            f"ERROR[{point.row + 1}:{point.column}] "
            f"{text}")

    @abstractmethod
    def check_style(self) -> None:
        """
        Entry point for each checker.
        This method should hold custom logic of checking files
        """

class CChecker(Checker):
    """
    Checker class for analyzing and processing syntax trees for c source files.
    """
    def check_style(self) -> None:
        for node in self.captures["function.body"]:
            self.__check_function_body(node)

    def __check_function_body(self, node: Node):
        # Indents are checked relative to the curent node.
        indent: int = node.children[0].start_point.column

        # The first child should be unnamed.  Here begins the function body block
        # Function body should always start with indent 0
        if node.children[0].text == b'{' or not node.children[0].is_named:
            if indent > 0:
                self.error(node.children[0].start_point, "Insufficient indentation")
        else:
            pass

        for n in iter(node.named_children):

            # Check indent for the child node. Function body should offset by 2 spaces
            # Handle comments separately
            if n.start_point.column > indent + 2 and n.type != 'comment':
                self.error(n.start_point, "Insufficient indentation")

            match n.type:
                case 'if_statement':
                    self.__check_if_statement(n)
                case 'for_statement':
                    pass
                case 'while_statement':
                    pass
                case 'expression_statement':
                    pass
                case _:
                    pass

        # The last child should be unnamed.  Here ends the function body block
        if node.children[-1].text != b'}' or node.children[-1].is_named:
            if indent > 0:
                self.error(node.children[0].start_point, "Insufficient indentation")

    def  __check_if_statement(self, node: Node) -> None:
        indent: int = node.start_point.column

        # Unwrap children
        if node.child_count == 4:
            # If case
            if_keyword, condition, consequence, alternative = node.children
        else:
            # If-else case
            if_keyword, condition, consequence = node.children

        print(condition.child_by_field_name("<"))

        # Between keyword end and parentheses there should be an whitespace
        if (condition.start_point.column - if_keyword.end_point.column) != 1:
            self.error(if_keyword.end_point, "Missing whitespace after keyword")

        # Never space after the left parentheses
        if (condition.children[1].start_point.column -
            condition.children[0].end_point.column) != 0:
            self.error(condition.children[0].end_point, "Missing whitespace after keyword")

        # Never space before the right parentheses
        if (condition.children[-1].start_point.column -
            condition.children[-2].end_point.column) != 0:
            self.error(condition.children[-1].end_point, "Missing whitespace after keyword")

        # Open braket should be on separate line
        if consequence.start_point.row == condition.start_point.row:
            self.error(condition.start_point, "Left bracket not on separate line")

        # Open braket should be indented
        if consequence.start_point.column != indent + 2:
            self.error(consequence.start_point, "Insufficient indentation")
            
        

class NxStyle():
    """
    NxTool command class for nuttx code checker.
    This class handles subparser named "nxstyle"
    It configures the argparser arguments and entrypoint 
    
    :param subparser: The subparser instance given by the NxTool class
    :type subparser: _SubParsersAction[ArgumentParser]
    """
    def __init__(self, subparser) -> None:
        self.argparser = subparser.add_parser(
            "nxstyle",
            help="Greet the user"
        )

        self.argparser.add_argument(
            "--graph",
            help="save dot language graph",
            type=Path,
            required=False
        )

        self.argparser.add_argument(
            "file",
            type=Path,
            help="File to check"
        )

        self.argparser.set_defaults(func=self.__run)

        self.checker: CChecker | None = None

    def __run(self, args: Namespace) -> None:
        """
        This method is an entrypoint for "nxtyle" subcommand.
        It gets called indirectly using ```args.func(args)```
        from NxTool class
        
        :param args: Attribute storage resolved by argparge
        :type args: Namespace
        """
        file_path = Path(args.file).as_posix()
        file_extension = Path(args.file).suffix

        try:
            with open(file_path, 'rb') as fd:
                src = fd.read()
        except FileNotFoundError as e:
            print(f"{e}")
            sys.exit(1)

        match file_extension:
            case '.c':
                lang = get_language('c')
                parser = get_parser('c')
                tree = parser.parse(src)
                self.checker = CChecker(tree, parser, lang, 'c.scm')

            case '.h':
                lang = get_language('c')
                parser = get_parser('c')
                tree = parser.parse(src)

            case _:
                sys.exit(1)

        if args.graph is not None:
            graph_path = args.graph.as_posix()
            self.checker.gen_dot_graph(graph_path)
            return

        self.checker.check_style()
