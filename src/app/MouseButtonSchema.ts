import { z } from "zod";

export const MouseButtonSchema = z.enum(["left", "right", "middle"]);
