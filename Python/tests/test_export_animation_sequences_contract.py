import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
PLUGIN_ROOTS = (
    ROOT / "UnrealMCP",
    ROOT / "FlopperamUnrealMCP" / "Plugins" / "UnrealMCP",
)


class ExportAnimationSequencesContractTests(unittest.TestCase):
    def test_local_server_exposes_reference_pose_sequence_creation_tool(self):
        server = (ROOT / "Python" / "unreal_mcp_server_advanced.py").read_text(encoding="utf-8")

        self.assertIn("def create_reference_pose_animation_sequence(", server)
        self.assertIn('"create_reference_pose_animation_sequence"', server)
        self.assertIn('"skeletal_mesh_path"', server)
        self.assertIn('"destination_path"', server)

    def test_bundled_plugins_route_reference_pose_sequence_creation_command(self):
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

                self.assertIn('TEXT("create_reference_pose_animation_sequence")', bridge)
                self.assertIn("HandleCreateReferencePoseAnimationSequence", header)
                self.assertIn("HandleCreateReferencePoseAnimationSequence", editor_commands)
                self.assertIn("UAnimSequenceFactory", editor_commands)
                self.assertIn("CreateAnimation", editor_commands)
                self.assertIn("SetPreviewMesh", editor_commands)

    def test_local_server_exposes_direct_anim_sequence_export_tool(self):
        server = (ROOT / "Python" / "unreal_mcp_server_advanced.py").read_text(encoding="utf-8")

        self.assertIn("def export_animation_sequences(", server)
        self.assertIn('"export_animation_sequences"', server)
        self.assertIn('"animation_paths"', server)
        self.assertIn('"fbx_output_dir"', server)
        self.assertIn('"source_id"', server)
        self.assertIn('"clip_kind"', server)
        self.assertIn('"loop"', server)

    def test_bundled_plugins_route_direct_anim_sequence_export_command(self):
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

                self.assertIn('TEXT("export_animation_sequences")', bridge)
                self.assertIn("HandleExportAnimationSequences", header)
                self.assertIn("HandleExportAnimationSequences", editor_commands)
                self.assertIn("UAnimSequenceExporterFBX", editor_commands)
                self.assertIn("UAssetExportTask", editor_commands)
                self.assertIn("UExporter::RunAssetExportTask", editor_commands)
                self.assertIn('TEXT("sen_clip_metadata.json")', editor_commands)


if __name__ == "__main__":
    unittest.main()
