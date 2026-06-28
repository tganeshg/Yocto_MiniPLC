import { useEffect, useRef, useState, useCallback } from "react";

/*
 * usePoll --- generic polling hook.
 * Calls `fn` immediately and then every `intervalMs`. Cleans up on unmount
 * and skips overlapping calls. Framework glue only --- no domain logic.
 */
export function usePoll(fn, intervalMs = 2000, deps = []) {
  const [data, setData] = useState(null);
  const [error, setError] = useState(null);
  const [loading, setLoading] = useState(true);
  const inFlight = useRef(false);

  // eslint-disable-next-line react-hooks/exhaustive-deps
  const runner = useCallback(fn, deps);

  useEffect(() => {
    let alive = true;
    const tick = async () => {
      if (inFlight.current) return;
      inFlight.current = true;
      try {
        const result = await runner();
        if (alive) {
          setData(result);
          setError(null);
        }
      } catch (e) {
        if (alive) setError(e);
      } finally {
        inFlight.current = false;
        if (alive) setLoading(false);
      }
    };
    tick();
    const id = setInterval(tick, intervalMs);
    return () => {
      alive = false;
      clearInterval(id);
    };
  }, [runner, intervalMs]);

  return { data, error, loading };
}
