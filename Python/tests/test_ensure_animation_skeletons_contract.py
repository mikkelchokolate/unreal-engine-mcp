import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
PLUGIN_ROOTS = (
    ROOT / "UnrealMCP",
    ROOT / "FlopperamUnrealMCP" / "Plugins" / "UnrealMCP",
)


class EnsureAnimationSkeletonsContractTests(unittest.TestCase):
    def test_local_server_exposes_ensure_animation_skeletons_tool(self):
        server = (ROOT / "Python" / "unreal_mcp_server_advanced.py").read_text(encoding="utf-8")

        self.assertIn("def ensure_animation_skeletons(", server)
        self.assertIn('"ensure_animation_skeletons"', server)
        self.assertIn('"mesh_skeleton_bindings"', server)
        self.assertIn('"skeletal_mesh_path"', server)
        self.assertIn('"skeleton_path"', server)
        self.assertIn('"animation_paths"', server)

    def test_bundled_plugins_route_ensure_animation_skeletons_to_editor_api(self):
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

                self.assertIn('TEXT("ensure_animation_skeletons")', bridge)
                self.assertIn("HandleEnsureAnimationSkeletons", header)
                self.assertIn("HandleEnsureAnimationSkeletons", editor_commands)
                self.assertIn("USkeletonFactory", editor_commands)
                self.assertIn("MergeAllBonesToBoneTree", editor_commands)
                self.assertIn("SkeletalMesh->SetSkeleton", editor_commands)
                self.assertIn("AnimationAsset->SetSkeleton", editor_commands)
                self.assertIn("SaveLoadedAsset", editor_commands)


if __name__ == "__main__":
    unittest.main()
