"""

"""
from argparse import Namespace
from pathlib import Path
import sys
from tree_sitter_language_pack import get_language, get_parser
import importlib.resources

from flake8.api import legacy as flake8
from nxstyle.nxstyle_main import Checker, CChecker

class NxStyle_cmd():
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

        self.checker: Checker | None = None

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

            case '.py':

                with importlib.resources.path("nxstyle.config", "setup.cfg") as config_path:
                    # Initialize the style guide with configuration from the file
                    style_guide = flake8.get_style_guide(config_file=config_path)

                # Run flake8 on the specified files
                style_guide.input_file(file_path.as_posix())

                sys.exit(0)

            case _:
                sys.exit(1)

        if self.checker is not None:
            self.checker.check_style()
