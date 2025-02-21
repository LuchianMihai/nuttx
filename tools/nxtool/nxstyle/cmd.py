"""

"""
from pathlib import Path
import tree_sitter_c as tsc
from tree_sitter import Language, Parser, Tree

class NxStyle():
    def __init__(self, subparser):
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

        self.parser: Parser | None = None
        self.tree: Tree | None = None

    def __run(self, args):
        file_path = args.file.as_posix()
        self.__gen_tree(file_path)

        if args.graph is not None:
            graph_path = args.graph.as_posix()
            self.__gen_dot_graph(graph_path)

    def __gen_tree(self, file_path: Path):
        try:
            with open(file_path, 'rb') as fd:
                src = fd.read()
                c_lang = Language(tsc.language())
                self.parser = Parser(c_lang)
                self.tree = self.parser.parse(src)
        except FileNotFoundError as e:
            print(f"{e}")

    def __gen_dot_graph(self, graph_path: Path):
        try:
            with open(graph_path, 'wb') as gfd:
                self.tree.print_dot_graph(gfd)
        except FileNotFoundError as e:
            print(f"{e}")

    def __check_style(self, src):
        pass