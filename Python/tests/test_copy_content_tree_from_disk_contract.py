import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
PLUGIN_ROOTS = (
    ROOT / "UnrealMCP",
    ROOT / "FlopperamUnrealMCP" / "Plugins" / "UnrealMCP",
)


class CopyContentTreeFromDiskContractTests(unittest.TestCase):
    def test_local_server_exposes_copy_content_tree_from_disk_tool(self):
        server = (ROOT / "Python" / "unreal_mcp_server_advanced.py").read_text(encoding="utf-8")

        self.assertIn("def copy_content_tree_from_disk(", server)
        self.assertIn('"copy_content_tree_from_disk"', server)
        self.assertIn('"source_content_dir"', server)
        self.assertIn('"destination_path"', server)
        self.assertIn('"replace_existing"', server)

    def test_bundled_plugins_route_copy_content_tree_from_disk_command(self):
        for plugin_root in PLUGIN_ROOTS:
            with self.subTest(plugin_root=plugin_root):
                header = (
                    plugin_root
                    / "Source"
                    / "UnrealMCP"
                    / "Public"
                    / "Commands"
                    / "EpicUnrealMCPEditorCommands.h"
                ).read_text(encoding="utf-8")
                editor_commands = (
                    plugin_root
                    / "Source"
                    / "UnrealMCP"
                    / "Private"
                    / "Commands"
                    / "EpicUnrealMCPEditorCommands.cpp"
                ).read_text(encoding="utf-8")
                bridge = (
                    plugin_root / "Source" / "UnrealMCP" / "Private" / "EpicUnrealMCPBridge.cpp"
                ).read_text(encoding="utf-8")

                self.assertIn('TEXT("copy_content_tree_from_disk")', bridge)
                self.assertIn("HandleCopyContentTreeFromDisk", header)
                self.assertIn("HandleCopyContentTreeFromDisk", editor_commands)
                self.assertIn("FindFilesRecursive", editor_commands)
                self.assertIn("TryConvertLongPackageNameToFilename", editor_commands)
                self.assertIn("FPaths::ConvertRelativePathToFull(DestinationPhysicalDir)", editor_commands)
                self.assertIn("FPaths::NormalizeFilename(SourceContentDir)", editor_commands)
                self.assertNotIn("MakeStandardFilename(SourceContentDir)", editor_commands)
                self.assertNotIn("MakeStandardFilename(NormalizedSourceFile)", editor_commands)
                self.assertIn("FPaths::GetBaseFilename(RelativePath)", editor_commands)
                self.assertNotIn('FPaths::SetExtension(RelativePackagePath, TEXT(""))', editor_commands)
                self.assertIn("ScanPathsSynchronous", editor_commands)
                self.assertIn('TEXT("copied_asset_paths")', editor_commands)
                self.assertIn('TEXT("skipped_files")', editor_commands)


if __name__ == "__main__":
    unittest.main()
