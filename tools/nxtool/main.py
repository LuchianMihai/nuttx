#!/usr/bin/env python3
import argparse
from nxstyle.cmd import NxStyle

class NxTool():
    def __init__(self):
        self.argparser = argparse.ArgumentParser(description="My CLI tool")

        self.argsubparser = self.argparser.add_subparsers(
            title='commands',
            required=True
        )

        self.nxstyle: NxStyle = NxStyle(self.argsubparser)

    def run(self):
        """
        NxTool command entry point
        """
        args = self.argparser.parse_args()
        args.func(args)

nxt = NxTool()
nxt.run()
