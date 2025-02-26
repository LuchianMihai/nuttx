"""

"""
from pathlib import Path
import importlib.resources
import sys
from abc import ABC, abstractmethod
from typing import Generator
from argparse import Namespace

from tree_sitter import Language, Parser, Tree, Node, Point, Query
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
    def __init__(self, file: Path, tree: Tree, parser: Parser, lang: Language, scm: str):
        self.file: Path = file
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
            f"{self.file.resolve()}:{point.row + 1}:{point.column}: "
            f"[INFO] "
            f"{text}")

    def warning(self, point: Point, text: str) -> None:
        print(
            f"{self.file.resolve()}:{point.row + 1}:{point.column}: "
            f"[WARNING] "
            f"{text}")

    def error(self, point: Point, text: str) -> None:
        print(
            f"{self.file.resolve()}:{point.row + 1}:{point.column}: "
            f"[ERROR] "
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
        for m in self.captures["function.body"]:
            for n in iter(m.named_children):
                self.__check_indents(2, n)

        for m in self.captures["statement.if"]:
            self.__check_if_statement(m)

    def __check_indents(self, indent: int, node: Node):

        match node.type:
            case "if_statement":
                for n in node.child_by_field_name("consequence").named_children:
                    self.__check_indents(indent + 4, n)
            case "while_statement":
                for n in node.child_by_field_name("body").named_children:
                    self.__check_indents(indent + 4, n)
            case "for_statement":
                for n in node.child_by_field_name("body").named_children:
                    self.__check_indents(indent + 4, n)
            case "while_statement":
                for n in node.child_by_field_name("body").named_children:
                    self.__check_indents(indent + 4, n)
            case "switch_statement":
                for n in node.child_by_field_name("body").named_children:
                    self.__check_indents(indent + 4, n)
            case (
                "return_statement" |
                "expression_statement"
            ):
                for child in node.named_children:
                    self.__check_indents(indent + 2, child)
            case _:
                return

        if node.start_point.column != indent:
            self.error(node.start_point, "Wrong indentation")

    def  __check_if_statement(self, node: Node) -> None:
        
        if (
            node.prev_sibling is not None and
            node.prev_sibling.type == 'else'
        ):
            indent: int = node.prev_sibling.start_point.column
        else:
            indent: int = node.start_point.column

        # unwrap if statement node
        if_keyword: Node | None = node.child(0)
        condition: Node | None = node.child_by_field_name("condition")
        consequence: Node | None = node.child_by_field_name("consequence")
        alternative: Node | None = node.child_by_field_name("alternative")


        # Between keyword end and parentheses there should be an whitespace
        if (condition.start_point.column - if_keyword.end_point.column) != 1:
            self.error(if_keyword.end_point, "Missing whitespace after keyword")

        # Open braket should be on separate line
        if consequence.start_point.row == condition.start_point.row:
            self.error(condition.start_point, "Left bracket not on separate line")

        # Open braket should be indented
        if consequence.start_point.column != indent + 2:
            print(consequence.text.decode())
            self.error(consequence.start_point, f"Insufficient indentation {consequence.start_point.column} : {indent + 2}" )

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
        file_path = Path(args.file)

        try:
            with open(file_path.as_posix(), 'rb') as fd:
                src = fd.read()
        except FileNotFoundError as e:
            print(f"{e}")
            sys.exit(1)

        match file_path.suffix:
            case '.c':
                lang = get_language('c')
                parser = get_parser('c')
                tree = parser.parse(src)
                self.checker = CChecker(file_path, tree, parser, lang, 'c.scm')

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
