"""
ffvoice.mcp — MCP (Model Context Protocol) server for ffvoice-engine.

Exposes offline speech-recognition capabilities as MCP tools so that AI
agents (e.g. Claude Desktop) can invoke them without any external services.

Usage::

    # Start the server (stdio transport):
    ffvoice-mcp

    # Or from Python:
    from ffvoice.mcp import main
    main()
"""

from __future__ import annotations

try:
    from mcp.server.fastmcp import FastMCP as _FastMCP  # noqa: F401
except ImportError as _err:
    raise ImportError(
        "The 'mcp' package is required to use ffvoice.mcp. "
        "Install it with:  pip install 'ffvoice[mcp]'"
    ) from _err

from ffvoice.mcp.server import main, mcp

__all__ = ["mcp", "main"]
