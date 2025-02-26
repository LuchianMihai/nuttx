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
            f"INFO[{point.row}:{point.column}] "
            f"{text}")

    def warning(self, point: Point, text: str) -> None:
        print(
            f"WARNING[{point.row}:{point.column}] "
            f"{text}")

    def error(self, point: Point, text: str) -> None:
        print(
            f"ERROR[{point.row}:{point.column}] "
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
        if node.children[0].text != b'{' or node.children[0].is_named:
            pass

        for n in iter(node.children[1:-1]):
            match n.type:
                case 'if_statement':
                    pass
                case 'for_statement':
                    pass
                case 'while_statement':
                    pass
                case 'expression_statement':
                    pass
                case _:
                    pass

        if node.children[-1].text != b'}' or node.children[0].is_named:
            pass

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
