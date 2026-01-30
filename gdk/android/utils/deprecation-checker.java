// Invoke as java -cp $ANDROID_HOME/platforms/android-$SDKVER/android.jar gdk/android/utils/deprecation-checker.java < gdk/android/gdkandroidinit.c 

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.IOException;
import java.lang.reflect.AnnotatedElement;
import java.lang.reflect.Array;
import java.util.ArrayList;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class DeprecationCheckParser {
	private static Pattern patFindClass = Pattern.compile("FindClass\\s*\\(\\s*env\\s*,\\s*\"([^\"]+)\"\\s*\\)");

	private static Pattern patMethod = Pattern.compile("POPULATE_REFCACHE_METHOD\\s*\\(.*,.*,\\s*\"(.*)\"\\s*,\\s*\"(.*)\"\\s*\\)");
	private static Pattern patStaticMethod = Pattern.compile("POPULATE_STATIC_REFCACHE_METHOD\\s*\\(.*,.*,\\s*\"(.*)\"\\s*,\\s*\"(.*)\"\\s*\\)");
	private static Pattern patMember = Pattern.compile("POPULATE_REFCACHE_MEMBER\\s*\\(.*,.*,\\s*\"(.*)\"\\s*,\\s*\"(.*)\"\\s*\\)");
	private static Pattern patField = Pattern.compile("POPULATE_REFCACHE_FIELD\\s*\\(.*,.*,\\s*\"(.*)\"\\s*\\)");

	private static Class<?> signaturePartToArg(String part) throws ClassNotFoundException {
		return switch (part.charAt(0)) {
			case 'I' -> int.class;
			case 'J' -> long.class;
			case 'D' -> double.class;
			case 'F' -> float.class;
			case 'Z' -> boolean.class;
			case 'B' -> byte.class;
			case 'C' -> char.class;
			case 'S' -> short.class;
			case 'L' -> {
				if (part.charAt(part.length() - 1) != ';') {
					throw new IllegalArgumentException("Bad signature part: " + part);
				}
				String name = part.substring(1, part.length() - 1).replace('/', '.');
				yield Class.forName(name);
			}
			default -> throw new IllegalArgumentException("Bad signature part: " + part);
		};
	}
	private static Class<?>[] signatureToArgs(String signature) throws ClassNotFoundException {
		List<Class<?>> args = new ArrayList<>();
		int i = 1; // skip '('
		while (signature.charAt(i) != ')') {
			int arrayDepth = 0;

			while (signature.charAt(i) == '[') {
				arrayDepth++;
				i++;
			}

			Class<?> baseClass;
			if (signature.charAt(i) == 'L') {
				int semicolon = signature.indexOf(';', i);
				String name = signature.substring(i + 1, semicolon).replace('/', '.');
				baseClass = Class.forName(name);
				i = semicolon + 1;
			} else {
				baseClass = signaturePartToArg(String.valueOf(signature.charAt(i)));
				i++;
			}

			for (int j = 0; j < arrayDepth; j++)
				baseClass = Array.newInstance(baseClass, 0).getClass();
			args.add(baseClass);
		}

		return args.toArray(Class<?>[]::new);
	}

	private static boolean isAnnotatedElementDeprecated(AnnotatedElement el) {
		return el.getAnnotation(Deprecated.class) != null;
	}

	private static boolean isMethodDeprecated(String klassName, String name, String signature) throws ClassNotFoundException, NoSuchMethodException {
		Class<?> klass = Class.forName(klassName.replace("/", "."));
		AnnotatedElement el;
		if (name.equals("<init>")) {
			el = klass.getConstructor(signatureToArgs(signature));
		} else {
			el = klass.getMethod(name, signatureToArgs(signature));
		}

		return isAnnotatedElementDeprecated(el);
	}
	private static boolean isFieldDeprecated(String klassName, String name, String signature) throws ClassNotFoundException, NoSuchFieldException {
		Class<?> klass = Class.forName(klassName.replace("/", "."));
		AnnotatedElement el = klass.getField(name);

		return isAnnotatedElementDeprecated(el);
	}

	public static void main(String[] args) throws IOException, ClassNotFoundException, NoSuchMethodException, NoSuchFieldException {
		BufferedReader reader = new BufferedReader(new InputStreamReader(System.in));

		boolean inBlock = false;
		String currentClass = null;

		String line;
		while ((line = reader.readLine()) != null) {
			if (line.contains("BEGIN DEPRECATION CHECK")) {
				inBlock = true;
				continue;
			}
			if (line.contains("END DEPRECATION CHECK")) {
				inBlock = false;
				continue;
			}

			if (!inBlock)
				continue;

			Matcher matchFindClass = patFindClass.matcher(line);
			if (matchFindClass.find()) {
				currentClass = matchFindClass.group(1);
				continue;
			} else if (line.strip().isEmpty()) {
				currentClass = null;
				continue;
			}

			if (currentClass != null) {
				Matcher m;

				m = patMethod.matcher(line);
				if (m.find()) {
					String name = m.group(1);
					String sig = m.group(2);
					if (isMethodDeprecated(currentClass, name, sig))
						System.err.printf("Method %s%s is deprecated\n", name, sig);
				}

				m = patStaticMethod.matcher(line);
				if (m.find()) {
					String name = m.group(1);
					String sig = m.group(2);
					if (isMethodDeprecated(currentClass, name, sig))
						System.err.printf("Static method %s%s is deprecated\n", name, sig);
				}

				m = patMember.matcher(line);
				if (m.find()) {
					String name = m.group(1);
					String sig = m.group(2);
					if (isFieldDeprecated(currentClass, name, sig))
						System.err.printf("Member %s (%s) is deprecated\n", name, sig);
				}

				m = patField.matcher(line);
				if (m.find()) {
					String name = m.group(1);
					if (isFieldDeprecated(currentClass, name, "I"))
						System.err.printf("Field %s is deprecated\n", name);
				}
			}
		}
	}
}
