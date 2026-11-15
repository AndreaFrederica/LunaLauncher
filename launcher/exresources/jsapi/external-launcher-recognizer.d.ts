/** Luna Launcher external launcher provenance recognizer API. */
export interface ExternalLauncherProbeContext {
  root: string;
  version: Record<string, unknown>;
  layout: {
    gameRoot: string;
    versionJson: string;
    isolated: boolean;
  };
}

export interface ExternalLauncherDetection {
  confidence: number;
  version?: string;
  matchedPaths?: string[];
  matchedFields?: string[];
  capabilities?: {
    read?: boolean;
    editCanonicalMetadata?: boolean;
  };
}

export interface ExternalLauncherRecognizer {
  id: string;
  displayName: string;
  iconId: string;
  priority?: number;
  detect(context: ExternalLauncherProbeContext): ExternalLauncherDetection;
}

/** A recognizer script must define this global. */
declare const recognizer: ExternalLauncherRecognizer;

